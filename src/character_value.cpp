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
    optional( jo, was_loaded, "desc", desc );
    optional( jo, was_loaded, "hardcode_value", hardcode_value,
              enum_flags_reader<hardcode_character_value>( "hardcode_character_value" ),
              hardcode_character_value::none );
    optional( jo, was_loaded, "var_value", character_var_value, "" );
    optional( jo, was_loaded, "min_value", min_value, INT_MIN );
    optional( jo, was_loaded, "max_value", max_value, INT_MAX );
}

void character_value::check() const
{
    if( character_var_value.empty() && hardcode_value == hardcode_character_value::none ) {
        debugmsg( "Character Value %s lacks any value, it will always be 0.", id.str() );
    }
    if( !character_var_value.empty() && hardcode_value != hardcode_character_value::none ) {
        debugmsg( "Character Value %s has two values, defaulting to only %s.", id.str(),
                  character_var_value );
    }
    if( min_value > max_value ) {
        debugmsg( "Character Value %s has greater min value then max value, it will be capped to `min_value`",
                  id.str() );
    }
}

void character_value::check_consistency() { all_character_values.check(); }


auto character_value::get_all() -> std::vector<character_value>
{
    return all_character_values.get_all();
}

void character_value::reset() { all_character_values.reset(); }

int character_value::get_amount( const Character &ch ) const
{
    int res = 0;
    if( character_var_value != "" ) {
        ch.get_value( character_var_value, res );
    }
    switch( hardcode_value ) {
        case hardcode_character_value::strength:
            res = ch.get_str();
            break;
        case hardcode_character_value::dexterity:
            res = ch.get_dex();
            break;
        case hardcode_character_value::intelligence:
            res = ch.get_int();
            break;
        case hardcode_character_value::perception:
            res = ch.get_per();
            break;
        case hardcode_character_value::health_points: {
            int max = 0;
            for( const std::pair<const bodypart_str_id, bodypart> &elem : ch.get_body() ) {
                max = std::max( max, elem.second.get_hp_cur() );
            }
            res = max;
            break;
        }
        case hardcode_character_value::mana:
            res = ch.magic->available_mana();
            break;
        case hardcode_character_value::stamina:
            res = ch.get_stamina();
            break;
        case hardcode_character_value::bionic_power:
            res = units::to_kilojoule( ch.get_power_level() );
            break;
        case hardcode_character_value::fatigue:
            res = ch.get_fatigue();
            break;
        case hardcode_character_value::thirst:
            res = ch.get_thirst();
            break;
        case hardcode_character_value::pain:
            res = ch.get_pain();
            break;
        case hardcode_character_value::radiation:
            res = ch.get_rad();
            break;
        case hardcode_character_value::health:
            res = ch.get_healthy();
            break;
        case hardcode_character_value::hunger:
            res = ceil( 100 - ch.get_kcal_percent() * 100 );
            break;
        case hardcode_character_value::morale:
            res = ch.get_morale_level();
            break;
        case hardcode_character_value::focus:
            res = ch.focus_pool;
            break;
        case hardcode_character_value::none:
            res = 0;
            break;
    }
    return std::max( std::min( res, max_value ), min_value );
}

std::string character_value::get_desc() const
{
    return desc.translated();
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
