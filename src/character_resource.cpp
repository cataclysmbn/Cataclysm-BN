#include "character_resource.h"

#include "assign.h"
#include "cached_options.h"
#include "character.h"
#include "debug.h"
#include "enum_conversions.h"
#include "flag.h"
#include "itype.h"
#include "generic_factory.h"
#include "generic_readers.h"
#include "mtype.h"
#include "monster.h"
#include "rng.h"
#include "type_id.h"
#include "type_id_implement.h"
#include "units_energy.h"
#include "visitable.h"

#include <optional>
#include <vector>

namespace
{
generic_factory<character_resource> all_character_resources( "Character Resource" );
}

IMPLEMENT_STRING_AND_INT_IDS( character_resource, all_character_resources );

void character_resource::load_character_resources( const JsonObject &jo, const std::string &src )
{
    all_character_resources.load( jo, src );
}

void character_resource::load( const JsonObject &jo, const std::string &src )
{
    optional( jo, was_loaded, "need_info", need_info );
    optional( jo, was_loaded, "item_info", item_info );
    optional( jo, was_loaded, "drain_flag", drain_flag, auto_flags_reader<flag_id> {},
              flag_id::NULL_ID() );
    optional( jo, was_loaded, "drain_resource", drain_resource,
              enum_flags_reader<hardcode_character_resource>( "hardcode_character_resource" ),
              hardcode_character_resource::none );
    optional( jo, was_loaded, "charge_scalar", charge_scalar, 1.0f );
}

void character_resource::check() const
{
    if( !drain_flag.is_valid() && !drain_flag.is_null() ) {
        debugmsg( "Character Resource %s has invalid non-null drain flag %s", id.str(), drain_flag.str() );
    }
    if( drain_flag.is_null() && drain_resource == hardcode_character_resource::none ) {
        debugmsg( "Character resource %s lacks a resource.", id.str() );
    }
}

void character_resource::check_consistency() { all_character_resources.check(); }


auto character_resource::get_all() -> std::vector<character_resource>
{
    return all_character_resources.get_all();
}

void character_resource::reset() { all_character_resources.reset(); }

/** Sums the two terms, being careful to not trigger overflow.
 * Doesn't handle underflow.
 *
 * @param a The first addend.
 * @param b The second addend.
 * @return the sum of the addends, but truncated to std::numeric_limits<int>::max().
 */
template <typename T>
static T sum_no_wrap( T a, T b )
{
    if( a > std::numeric_limits<T>::max() - b ||
        b > std::numeric_limits<T>::max() - a ) {
        return std::numeric_limits<T>::max();
    }
    return a + b;
}
bool character_resource::has_charges( const Character &ch, int amount ) const
{
    if( drain_flag.is_valid() ) {
        int qty = 0;
        if( drain_flag == flag_IS_UPS ) {
            if( ch.has_active_bionic( bionic_id( "bio_ups" ) ) ) {
                qty = sum_no_wrap( qty, int( floor( units::to_kilojoule( ch.get_power_level() ) *
                                                    charge_scalar ) ) );
            }
            if( ch.is_mounted() ) {
                auto mon = ch.mounted_creature.get();
                if( mon->has_flag( MF_RIDEABLE_MECH ) && mon->get_battery_item() ) {
                    qty = sum_no_wrap( qty, int( floor( mon->get_battery_item()->ammo_remaining() * charge_scalar ) ) );
                }
            }
        }
        ch.visit_items( [&]( const item * e ) {
            if( e->has_flag( drain_flag ) ) {
                // TODO: Maybe make a custom ups_eff_mult esque value for this?
                qty = sum_no_wrap( qty, int( round( e->ammo_remaining() * charge_scalar ) ) );
            }
            return qty < amount ? VisitResponse::NEXT : VisitResponse::ABORT;
        } );
        return qty >= amount;
    }
    if( drain_resource != hardcode_character_resource::none ) {
        switch( drain_resource ) {
            case hardcode_character_resource::health_points:
                for( const std::pair<const bodypart_str_id, bodypart> &elem : ch.get_body() ) {
                    if( elem.second.get_hp_cur() * charge_scalar > amount ) { return true; }
                }
                return false;
            case hardcode_character_resource::mana:
                return ch.magic->available_mana() * charge_scalar >= amount;
            case hardcode_character_resource::stamina:
                return ch.get_stamina() * charge_scalar >= amount;
            case hardcode_character_resource::bionic:
                return ch.get_power_level() * charge_scalar >= units::from_kilojoule( amount );
            case hardcode_character_resource::fatigue:
                return ch.get_fatigue() < fatigue_levels::exhausted;
            case hardcode_character_resource::thirst:
                // You could technically do thirst_levels::dead here, but that seems insane
                return ch.get_thirst() < thirst_levels::parched;
            case hardcode_character_resource::pain:
                // One can always be in more pain
                return true;
            case hardcode_character_resource::radiation:
                // One can always be more irradiated
                return true;
            case hardcode_character_resource::health:
                return ch.get_healthy() * charge_scalar >= amount;
            case hardcode_character_resource::hunger:
                return ch.get_stored_kcal() * charge_scalar >= amount;
        }
    }
    return false;
}

int character_resource::get_charges( const Character &ch ) const
{
    if( drain_flag.is_valid() ) {
        int qty = 0;
        if( drain_flag == flag_IS_UPS ) {
            if( ch.has_active_bionic( bionic_id( "bio_ups" ) ) ) {
                qty = sum_no_wrap( qty, int( floor( units::to_kilojoule( ch.get_power_level() ) *
                                                    charge_scalar ) ) );
            }
            if( ch.is_mounted() ) {
                auto mon = ch.mounted_creature.get();
                if( mon->has_flag( MF_RIDEABLE_MECH ) && mon->get_battery_item() ) {
                    std::cout << qty << "\n";
                    qty = sum_no_wrap( qty, int( floor( mon->get_battery_item()->ammo_remaining() * charge_scalar ) ) );
                }
            }
        }
        ch.visit_items( [&]( const item * e ) {
            if( e->has_flag( drain_flag ) ) {
                // TODO: Maybe make a custom ups_eff_mult esque value for this?
                qty = sum_no_wrap( qty, int( round( e->ammo_remaining() * charge_scalar *
                                                    e->type->tool->ups_eff_mult ) ) );
            }
            return VisitResponse::NEXT;
        } );
        return qty;
    }
    if( drain_resource != hardcode_character_resource::none ) {
        switch( drain_resource ) {
            case hardcode_character_resource::health_points: {
                float max = 0;
                for( const std::pair<const bodypart_str_id, bodypart> &elem : ch.get_body() ) {
                    max = std::max( max, elem.second.get_hp_cur() * charge_scalar );
                }
                return int( round( max ) );
            }
            case hardcode_character_resource::mana:
                return ch.magic->available_mana() * charge_scalar;
            case hardcode_character_resource::stamina:
                return ch.get_stamina() * charge_scalar;
            case hardcode_character_resource::bionic:
                return units::to_kilojoule( ch.get_power_level() ) * charge_scalar;
            case hardcode_character_resource::fatigue:
                // The / 2 is to prevent int overflows if it also has an internal battery
                return item::INFINITE_CHARGES / 2;
            case hardcode_character_resource::thirst:
                // You could technically do thirst_levels::dead here, but that seems insane
                return item::INFINITE_CHARGES / 2;
            case hardcode_character_resource::pain:
                // One can always be in more pain
                return item::INFINITE_CHARGES / 2;
            case hardcode_character_resource::radiation:
                // One can always be more irradiated
                return item::INFINITE_CHARGES / 2;
            case hardcode_character_resource::health:
                return ch.get_healthy() * charge_scalar;
            case hardcode_character_resource::hunger:
                return ch.get_stored_kcal() * charge_scalar;
            case hardcode_character_resource::none:
                return 0;
        }
    }
    return 0;
}

bool character_resource::drain_charges( Character &ch, int amount ) const
{
    const auto smooth_scale = [this]( double amount ) {
        return roll_remainder( amount / charge_scalar );
    };
    if( drain_flag.is_valid() ) {
        if( has_charges( ch, amount ) ) {
            int qty = amount;
            if( drain_flag == flag_IS_UPS ) {
                if( ch.is_mounted() && ch.mounted_creature.get()->has_flag( MF_RIDEABLE_MECH ) &&
                    ch.mounted_creature.get()->get_battery_item() ) {
                    auto mons = ch.mounted_creature.get();
                    int power_drain = std::min( mons->get_battery_item()->ammo_remaining(), qty );
                    mons->use_mech_power( smooth_scale( -power_drain ) );
                    qty -= std::min( qty, smooth_scale( power_drain ) );
                }
                if( ch.has_active_bionic( bionic_id( "bio_ups" ) ) ) {
                    int bio = std::min( units::to_kilojoule( ch.get_power_level() ), qty );
                    ch.mod_power_level( units::from_kilojoule( -smooth_scale( bio ) ) );
                    qty -= std::min( qty, bio );
                }
            }
            ch.remove_items_with( [ & ]( detached_ptr<item> &&e ) {
                if( e->has_flag( drain_flag ) && e->ammo_remaining() > 0 ) {
                    int eff_mult = charge_scalar * e->type->tool->ups_eff_mult;
                    detached_ptr<item> split = item::spawn( *e );
                    split->ammo_set( e->ammo_current(), e->ammo_remaining() );
                    int used = std::min( qty, e->ammo_remaining() * eff_mult );
                    qty -= used;
                    int rand_increase = x_in_y( used % eff_mult, eff_mult );
                    int really_used = ( used / eff_mult ) + rand_increase;
                    e->ammo_consume( really_used, ch.bub_pos() );
                }
                return qty != 0 ? VisitResponse::NEXT : VisitResponse::ABORT;
            } );
            return qty != 0;
        }
    }
    if( drain_resource != hardcode_character_resource::none ) {
        switch( drain_resource ) {
            case hardcode_character_resource::health_points:
                for( const std::pair<const bodypart_str_id, bodypart> &elem : ch.get_body() ) {
                    if( elem.second.get_hp_cur() * charge_scalar > amount ) {
                        return true;
                    }
                }
                return false;
            case hardcode_character_resource::mana:
                ch.magic->mod_mana( ch, smooth_scale( amount * -1 ) );
                return true;
            case hardcode_character_resource::stamina:
                ch.mod_stamina( smooth_scale( amount * -1 ) );
                return true;
            case hardcode_character_resource::bionic:
                ch.mod_power_level( units::from_joule( smooth_scale( amount * -1 * 1000 ) ) );
                return true;
            case hardcode_character_resource::fatigue:
                ch.mod_fatigue( smooth_scale( amount ) );
                return true;
            case hardcode_character_resource::thirst:
                ch.mod_thirst( smooth_scale( amount ) );
                return true;
            case hardcode_character_resource::pain:
                ch.mod_pain( smooth_scale( amount ) );
                return true;
            case hardcode_character_resource::radiation:
                ch.mod_rad( smooth_scale( amount ) );
                return true;
            case hardcode_character_resource::health:
                ch.mod_healthy( smooth_scale( amount * -1 ) );
                return true;
            case hardcode_character_resource::hunger:
                ch.mod_stored_kcal( smooth_scale( amount * -1 ) );
                return true;
        }
    }
    return false;
}


template<> struct enum_traits<character_resource::hardcode_character_resource> {
    static constexpr auto last = character_resource::hardcode_character_resource::none;
};
template<>
std::string io::enum_to_string<character_resource::hardcode_character_resource>
( character_resource::hardcode_character_resource res )
{
    switch( res ) {
        case character_resource::hardcode_character_resource::health_points:
            return "HP";
        case character_resource::hardcode_character_resource::mana:
            return "MANA";
        case character_resource::hardcode_character_resource::stamina:
            return "STAMINA";
        case character_resource::hardcode_character_resource::bionic:
            return "BIONIC";
        case character_resource::hardcode_character_resource::fatigue:
            return "FATIGUE";
        case character_resource::hardcode_character_resource::thirst:
            return "THIRST";
        case character_resource::hardcode_character_resource::pain:
            return "PAIN";
        case character_resource::hardcode_character_resource::radiation:
            return "RAD";
        case character_resource::hardcode_character_resource::health:
            return "HEALTH";
        case character_resource::hardcode_character_resource::hunger:
            return "HUNGER";
        case character_resource::hardcode_character_resource::none:
            return "NONE";
    }
}
