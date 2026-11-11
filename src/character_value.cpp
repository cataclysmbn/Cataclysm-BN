#include "character_value.h"

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
generic_factory<character_value> all_character_values( "Character Value" );
}

IMPLEMENT_STRING_AND_INT_IDS( character_value, all_character_values );

void character_value::load_character_values( const JsonObject &jo, const std::string &src )
{
    all_character_values.load( jo, src );
}

void character_value::load( const JsonObject &jo, const std::string &src )
{
    optional( jo, was_loaded, "hardcode_value", hardcode_value,
              enum_flags_reader<hardcode_character_value>( "hardcode_character_value" ),
              hardcode_character_value::none );
    optional( jo, was_loaded, "var_value", character_var_value, "" );
}

void character_value::check() const
{
    if( character_var_value == "" && hardcode_value == hardcode_character_value::none ) {
        debugmsg( "Character Value %s lacks any value, it will always be 0.", id.str() );
    }
}

void character_value::check_consistency() { all_character_values.check(); }


auto character_value::get_all() -> std::vector<character_value>
{
    return all_character_values.get_all();
}

void character_value::reset() { all_character_values.reset(); }

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

int character_value::get_amount( const Character &ch ) const
{
    if( character_var_value != "" ) {
        int def = 0;
        return ch.get_value( character_var_value, def );
    }
    switch( hardcode_value ) {
        case hardcode_character_value::strength:
            return ch.get_str();
        case hardcode_character_value::dexterity:
            return ch.get_dex();
        case hardcode_character_value::intelligence:
            return ch.get_int();
        case hardcode_character_value::perception:
            return ch.get_per();
        case hardcode_character_value::health_points: {
            int max = 0;
            for( const std::pair<const bodypart_str_id, bodypart> &elem : ch.get_body() ) {
                max = std::max( max, elem.second.get_hp_cur() );
            }
            return max;
        }
        case hardcode_character_value::mana:
            return ch.magic->available_mana();
        case hardcode_character_value::stamina:
            return ch.get_stamina();
        case hardcode_character_value::bionic_power:
            return units::to_kilojoule( ch.get_power_level() );
        case hardcode_character_value::fatigue:
            // The / 2 is to prevent int overflows if it also has an internal battery
            return item::INFINITE_CHARGES / 2;
        case hardcode_character_value::thirst:
            // You could technically do thirst_levels::dead here, but that seems insane
            return item::INFINITE_CHARGES / 2;
        case hardcode_character_value::pain:
            // One can always be in more pain
            return item::INFINITE_CHARGES / 2;
        case hardcode_character_value::radiation:
            // One can always be more irradiated
            return item::INFINITE_CHARGES / 2;
        case hardcode_character_value::health:
            return ch.get_healthy();
        case hardcode_character_value::hunger:
            return ch.get_stored_kcal();
        case hardcode_character_value::morale:
            return ch.get_morale_level();
        case hardcode_character_value::focus:
            return ch.focus_pool;
        case hardcode_character_value::none:
            return 0;
    }
    return 0;
}

template<> struct enum_traits<character_value::hardcode_character_value> {
    static constexpr auto last = character_value::hardcode_character_value::none;
};
template<>
std::string io::enum_to_string<character_value::hardcode_character_value>
( character_value::hardcode_character_value res )
{
    switch( res ) {
        case character_value::hardcode_character_value::strength:
            return "STR";
        case character_value::hardcode_character_value::dexterity:
            return "DEX";
        case character_value::hardcode_character_value::intelligence:
            return "INT";
        case character_value::hardcode_character_value::perception:
            return "PER";
        case character_value::hardcode_character_value::health:
            return "HEALTH";
        case character_value::hardcode_character_value::health_points:
            return "HP";
        case character_value::hardcode_character_value::mana:
            return "MANA";
        case character_value::hardcode_character_value::stamina:
            return "STAMINA";
        case character_value::hardcode_character_value::bionic_power:
            return "BIONIC_POWER";
        case character_value::hardcode_character_value::fatigue:
            return "FATIGUE";
        case character_value::hardcode_character_value::thirst:
            return "THIRST";
        case character_value::hardcode_character_value::pain:
            return "PAIN";
        case character_value::hardcode_character_value::radiation:
            return "RAD";
        case character_value::hardcode_character_value::hunger:
            return "HUNGER";
        case character_value::hardcode_character_value::morale:
            return "MORALE";
        case character_value::hardcode_character_value::focus:
            return "FOCUS";
        case character_value::hardcode_character_value::none:
            return "NONE";
    }
}
