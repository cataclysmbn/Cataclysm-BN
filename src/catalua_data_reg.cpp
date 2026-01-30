#include "catalua_data_reg.h"

#include "bionics.h"
#include "calendar.h"
#include "catalua_impl.h"
#include "character_stat.h"
#include "damage.h"
#include "debug.h"
#include "dialogue.h"
#include "effect.h"
#include "emit.h"
#include "enums.h"
#include "flag.h"
#include "generic_factory.h"
#include "json.h"
#include "lua_table_wrapper.h"
#include "magic.h"
#include "magic_enchantment.h"
#include "martialarts.h"
#include "material.h"
#include "morale_types.h"
#include "mutation.h"
#include "skill.h"
#include "translations.h"
#include "type_id.h"
#include "units.h"
#include "units_serde.h"
#include "vitamin.h"

#include <sstream>
#include <stdexcept>
#include <string>

namespace lua_data_helpers
{

/**
 * Helper functions for extracting values from Lua tables with type flexibility
 */
time_duration get_duration( const sol::table &def, const std::string &key,
                            time_duration default_val = 0_turns )
{
    sol::object val = def[key];
    if( !val.valid() || val.is<sol::lua_nil_t>() ) {
        return default_val;
    }
    if( val.is<time_duration>() ) {
        return val.as<time_duration>();
    }
    if( val.is<std::string>() ) {
        std::istringstream iss( val.as<std::string>() );
        JsonIn jsin( iss );
        return read_from_json_string<time_duration>( jsin, time_duration::units );
    }
    throw std::runtime_error( "Expected TimeDuration or string for " + key );
}

units::energy get_energy( const sol::table &def, const std::string &key,
                          units::energy default_val = 0_kJ )
{
    sol::object val = def[key];
    if( !val.valid() || val.is<sol::lua_nil_t>() ) {
        return default_val;
    }
    if( val.is<units::energy>() ) {
        return val.as<units::energy>();
    }
    if( val.is<std::string>() ) {
        std::istringstream iss( val.as<std::string>() );
        JsonIn jsin( iss );
        return read_from_json_string<units::energy>( jsin, units::energy_units );
    }
    throw std::runtime_error( "Expected Energy or string for " + key );
}

units::mass get_mass( const sol::table &def, const std::string &key,
                      units::mass default_val = 0_gram )
{
    sol::object val = def[key];
    if( !val.valid() || val.is<sol::lua_nil_t>() ) {
        return default_val;
    }
    if( val.is<units::mass>() ) {
        return val.as<units::mass>();
    }
    if( val.is<std::string>() ) {
        std::istringstream iss( val.as<std::string>() );
        JsonIn jsin( iss );
        return read_from_json_string<units::mass>( jsin, units::mass_units );
    }
    throw std::runtime_error( "Expected Mass or string for " + key );
}

template<typename T>
string_id<T> get_string_id( const sol::table &def, const std::string &key,
                            const string_id<T> &default_val = string_id<T>::NULL_ID() )
{
    sol::object val = def[key];
    if( !val.valid() || val.is<sol::lua_nil_t>() ) {
        return default_val;
    }
    if( val.is<string_id<T>>() ) {
        return val.as<string_id<T>>();
    }
    if( val.is<std::string>() ) {
        return string_id<T>( val.as<std::string>() );
    }
    throw std::runtime_error( "Expected ID or string for " + key );
}

template<typename T>
std::vector<string_id<T>> get_string_id_array( const sol::table &def, const std::string &key )
{
    std::vector<string_id<T>> result;
    sol::optional<sol::table> tbl = def[key];
    if( !tbl ) {
        return result;
    }
    for( auto &pair : *tbl ) {
        if( pair.second.is<string_id<T>>() ) {
            result.push_back( pair.second.template as<string_id<T>>() );
        } else if( pair.second.is<std::string>() ) {
            result.push_back( string_id<T>( pair.second.template as<std::string>() ) );
        }
    }
    return result;
}

std::map<bodypart_str_id, int> get_bodypart_int_map( const sol::table &def,
        const std::string &key )
{
    std::map<bodypart_str_id, int> result;
    sol::optional<sol::table> tbl = def[key];
    if( !tbl ) {
        return result;
    }
    for( auto &pair : *tbl ) {
        bodypart_str_id bp;
        if( pair.first.is<bodypart_str_id>() ) {
            bp = pair.first.as<bodypart_str_id>();
        } else if( pair.first.is<std::string>() ) {
            bp = bodypart_str_id( pair.first.as<std::string>() );
        } else {
            continue;
        }
        result[bp] = pair.second.as<int>();
    }
    return result;
}

translation get_translation( const sol::table &def, const std::string &key,
                             const translation &default_val = translation() )
{
    sol::object val = def[key];
    if( !val.valid() || val.is<sol::lua_nil_t>() ) {
        return default_val;
    }
    if( val.is<std::string>() ) {
        return translation::no_translation( val.as<std::string>() );
    }
    return default_val;
}

std::vector<std::string> get_string_array( const sol::table &def, const std::string &key )
{
    std::vector<std::string> result;
    sol::optional<sol::table> tbl = def[key];
    if( !tbl ) {
        return result;
    }
    for( auto &pair : *tbl ) {
        if( pair.second.is<std::string>() ) {
            result.push_back( pair.second.as<std::string>() );
        }
    }
    return result;
}

std::vector<translation> get_translation_array( const sol::table &def, const std::string &key )
{
    std::vector<translation> result;
    sol::optional<sol::table> tbl = def[key];
    if( !tbl ) {
        return result;
    }
    for( auto &pair : *tbl ) {
        if( pair.second.is<std::string>() ) {
            result.push_back( translation::no_translation( pair.second.as<std::string>() ) );
        }
    }
    return result;
}

std::string get_string( const sol::table &def, const std::string &key,
                        const std::string &default_val = "" )
{
    return def.get_or<std::string>( key, default_val );
}

bool get_bool( const sol::table &def, const std::string &key, bool default_val = false )
{
    return def.get_or<bool>( key, default_val );
}

int get_int( const sol::table &def, const std::string &key, int default_val = 0 )
{
    return def.get_or<int>( key, default_val );
}

float get_float( const sol::table &def, const std::string &key, float default_val = 0.0f )
{
    sol::object val = def[key];
    if( !val.valid() || val.is<sol::lua_nil_t>() ) {
        return default_val;
    }
    if( val.is<double>() ) {
        return static_cast<float>( val.as<double>() );
    }
    if( val.is<int>() ) {
        return static_cast<float>( val.as<int>() );
    }
    return default_val;
}

std::optional<float> get_optional_float( const sol::table &def, const std::string &key )
{
    sol::object val = def[key];
    if( !val.valid() || val.is<sol::lua_nil_t>() ) {
        return std::nullopt;
    }
    if( val.is<double>() ) {
        return static_cast<float>( val.as<double>() );
    }
    if( val.is<int>() ) {
        return static_cast<float>( val.as<int>() );
    }
    return std::nullopt;
}

std::set<std::string> get_string_set( const sol::table &def, const std::string &key )
{
    std::set<std::string> result;
    sol::optional<sol::table> tbl = def[key];
    if( !tbl ) {
        return result;
    }
    for( auto &pair : *tbl ) {
        if( pair.second.is<std::string>() ) {
            result.insert( pair.second.as<std::string>() );
        }
    }
    return result;
}

std::set<flag_id> get_flag_id_set( const sol::table &def, const std::string &key )
{
    std::set<flag_id> result;
    sol::optional<sol::table> tbl = def[key];
    if( !tbl ) {
        return result;
    }
    for( auto &pair : *tbl ) {
        if( pair.second.is<flag_id>() ) {
            result.insert( pair.second.as<flag_id>() );
        } else if( pair.second.is<std::string>() ) {
            result.insert( flag_id( pair.second.as<std::string>() ) );
        }
    }
    return result;
}

std::map<body_part, float> get_bodypart_float_map( const sol::table &def, const std::string &key )
{
    std::map<body_part, float> result;
    sol::optional<sol::table> tbl = def[key];
    if( !tbl ) {
        return result;
    }
    for( auto &pair : *tbl ) {
        std::string bp_str;
        if( pair.first.is<bodypart_str_id>() ) {
            bp_str = pair.first.as<bodypart_str_id>().str();
        } else if( pair.first.is<std::string>() ) {
            bp_str = pair.first.as<std::string>();
        } else {
            continue;
        }
        body_part bp = get_body_part_token( bp_str );
        float val = 0.0f;
        if( pair.second.is<double>() ) {
            val = static_cast<float>( pair.second.as<double>() );
        } else if( pair.second.is<int>() ) {
            val = static_cast<float>( pair.second.as<int>() );
        }
        result[bp] = val;
    }
    return result;
}

template<typename T>
std::map<string_id<T>, int> get_id_int_map( const sol::table &def, const std::string &key )
{
    std::map<string_id<T>, int> result;
    sol::optional<sol::table> tbl = def[key];
    if( !tbl ) {
        return result;
    }
    for( auto &pair : *tbl ) {
        string_id<T> id;
        if( pair.first.is<string_id<T>>() ) {
            id = pair.first.template as<string_id<T>>();
        } else if( pair.first.is<std::string>() ) {
            id = string_id<T>( pair.first.template as<std::string>() );
        } else {
            continue;
        }
        result[id] = pair.second.as<int>();
    }
    return result;
}

template<typename T>
std::set<string_id<T>> get_string_id_set( const sol::table &def, const std::string &key )
{
    std::set<string_id<T>> result;
    sol::optional<sol::table> tbl = def[key];
    if( !tbl ) {
        return result;
    }
    for( auto &pair : *tbl ) {
        if( pair.second.is<string_id<T>>() ) {
            result.insert( pair.second.template as<string_id<T>>() );
        } else if( pair.second.is<std::string>() ) {
            result.insert( string_id<T>( pair.second.template as<std::string>() ) );
        }
    }
    return result;
}

/**
 * Enum parsers
 */
game_message_type parse_game_message_type( const std::string &type )
{
    if( type == "good" ) {
        return m_good;
    } else if( type == "bad" ) {
        return m_bad;
    } else if( type == "mixed" ) {
        return m_mixed;
    } else if( type == "warning" ) {
        return m_warning;
    } else if( type == "info" ) {
        return m_info;
    } else if( type == "debug" ) {
        return m_debug;
    }
    return m_neutral;
}

character_stat parse_character_stat( const std::string &stat )
{
    if( stat == "STR" || stat == "str" || stat == "STRENGTH" ) {
        return character_stat::STRENGTH;
    } else if( stat == "DEX" || stat == "dex" || stat == "DEXTERITY" ) {
        return character_stat::DEXTERITY;
    } else if( stat == "INT" || stat == "int" || stat == "INTELLIGENCE" ) {
        return character_stat::INTELLIGENCE;
    } else if( stat == "PER" || stat == "per" || stat == "PERCEPTION" ) {
        return character_stat::PERCEPTION;
    }
    return character_stat::DUMMY_STAT;
}

std::map<character_stat, int> get_stat_bonus_map( const sol::table &def, const std::string &key )
{
    std::map<character_stat, int> result;
    sol::optional<sol::table> tbl = def[key];
    if( !tbl ) {
        return result;
    }
    for( auto &pair : *tbl ) {
        if( !pair.first.is<std::string>() ) {
            continue;
        }
        character_stat stat = parse_character_stat( pair.first.as<std::string>() );
        if( stat != character_stat::DUMMY_STAT ) {
            result[stat] = pair.second.as<int>();
        }
    }
    return result;
}

/**
 * Complex structure helpers
 */
std::vector<std::pair<std::string, int>> get_miss_messages( const sol::table &def,
                                      const std::string &key )
{
    std::vector<std::pair<std::string, int>> result;
    sol::optional<sol::table> tbl = def[key];
    if( !tbl ) {
        return result;
    }
    for( auto &pair : *tbl ) {
        if( pair.second.is<sol::table>() ) {
            sol::table entry = pair.second.as<sol::table>();
            sol::object msg_obj = entry[1];
            std::string msg = msg_obj.valid() && msg_obj.is<std::string>()
                              ? msg_obj.as<std::string>() : "";
            sol::object chance_obj = entry[2];
            int chance = chance_obj.valid() && chance_obj.is<int>()
                         ? chance_obj.as<int>() : 1;
            if( !msg.empty() ) {
                result.emplace_back( msg, chance );
            }
        }
    }
    return result;
}

std::vector<std::pair<std::string, game_message_type>> get_decay_messages( const sol::table &def,
        const std::string &key )
{
    std::vector<std::pair<std::string, game_message_type>> result;
    sol::optional<sol::table> tbl = def[key];
    if( !tbl ) {
        return result;
    }
    for( auto &pair : *tbl ) {
        if( pair.second.is<sol::table>() ) {
            sol::table entry = pair.second.as<sol::table>();
            std::string msg = entry.get_or<std::string>( 1, "" );
            std::string type_str = entry.get_or<std::string>( 2, "neutral" );
            if( !msg.empty() ) {
                result.emplace_back( msg, parse_game_message_type( type_str ) );
            }
        }
    }
    return result;
}

caused_effect get_caused_effect( const sol::table &tbl )
{
    caused_effect ce;

    sol::object type_obj = tbl["type"];
    if( type_obj.valid() ) {
        if( type_obj.is<efftype_id>() ) {
            ce.type = type_obj.as<efftype_id>();
        } else if( type_obj.is<std::string>() ) {
            ce.type = efftype_id( type_obj.as<std::string>() );
        }
    }

    sol::object intensity_req = tbl["intensity_requirement"];
    if( intensity_req.valid() && intensity_req.is<int>() ) {
        ce.intensity_requirement = intensity_req.as<int>();
    }

    sol::object allow_decay = tbl["allow_on_decay"];
    if( allow_decay.valid() && allow_decay.is<bool>() ) {
        ce.allow_on_decay = allow_decay.as<bool>();
    }

    sol::object allow_remove = tbl["allow_on_remove"];
    if( allow_remove.valid() && allow_remove.is<bool>() ) {
        ce.allow_on_remove = allow_remove.as<bool>();
    }

    sol::object duration_obj = tbl["duration"];
    if( duration_obj.valid() ) {
        if( duration_obj.is<time_duration>() ) {
            ce.duration = duration_obj.as<time_duration>();
        } else if( duration_obj.is<std::string>() ) {
            std::istringstream iss( duration_obj.as<std::string>() );
            JsonIn jsin( iss );
            ce.duration = read_from_json_string<time_duration>( jsin, time_duration::units );
        }
    }

    sol::object inherit_dur = tbl["inherit_duration"];
    if( inherit_dur.valid() && inherit_dur.is<bool>() ) {
        ce.inherit_duration = inherit_dur.as<bool>();
    }

    sol::object intensity = tbl["intensity"];
    if( intensity.valid() && intensity.is<int>() ) {
        ce.intensity = intensity.as<int>();
    }

    sol::object inherit_int = tbl["inherit_intensity"];
    if( inherit_int.valid() && inherit_int.is<bool>() ) {
        ce.inherit_intensity = inherit_int.as<bool>();
    }

    sol::object bp_obj = tbl["bp"];
    if( bp_obj.valid() ) {
        if( bp_obj.is<bodypart_str_id>() ) {
            ce.bp = bp_obj.as<bodypart_str_id>();
        } else if( bp_obj.is<std::string>() ) {
            ce.bp = bodypart_str_id( bp_obj.as<std::string>() );
        }
    }

    sol::object inherit_bp = tbl["inherit_body_part"];
    if( inherit_bp.valid() && inherit_bp.is<bool>() ) {
        ce.inherit_body_part = inherit_bp.as<bool>();
    }

    return ce;
}

std::vector<caused_effect> get_effects_on_remove( const sol::table &def, const std::string &key )
{
    std::vector<caused_effect> result;
    sol::optional<sol::table> tbl = def[key];
    if( !tbl ) {
        return result;
    }
    for( auto &pair : *tbl ) {
        if( pair.second.is<sol::table>() ) {
            result.push_back( get_caused_effect( pair.second.as<sol::table>() ) );
        }
    }
    return result;
}

void extract_lua_effect(
    const sol::table &tbl,
    std::unordered_map<std::tuple<std::string, bool, std::string, std::string>, double,
    cata::tuple_hash> &data,
    const std::string &mod_type, const std::string &data_key, const std::string &type_key,
    const std::string &arg_key )
{
    sol::object val_obj = tbl[mod_type];
    if( !val_obj.valid() || val_obj.is<sol::lua_nil_t>() ) {
        return;
    }

    double val = 0;
    double reduced_val = 0;

    if( val_obj.is<sol::table>() ) {
        sol::table arr = val_obj.as<sol::table>();
        sol::object first = arr[1];
        sol::object second = arr[2];
        if( first.valid() ) {
            if( first.is<double>() ) {
                val = first.as<double>();
            } else if( first.is<int>() ) {
                val = static_cast<double>( first.as<int>() );
            }
        }
        if( second.valid() && !second.is<sol::lua_nil_t>() ) {
            if( second.is<double>() ) {
                reduced_val = second.as<double>();
            } else if( second.is<int>() ) {
                reduced_val = static_cast<double>( second.as<int>() );
            }
        } else {
            reduced_val = val;
        }
    } else if( val_obj.is<double>() ) {
        val = val_obj.as<double>();
        reduced_val = val;
    } else if( val_obj.is<int>() ) {
        val = static_cast<double>( val_obj.as<int>() );
        reduced_val = val;
    }

    if( val != 0 ) {
        data[std::make_tuple( data_key, false, type_key, arg_key )] = val;
    }
    if( reduced_val != 0 ) {
        data[std::make_tuple( data_key, true, type_key, arg_key )] = reduced_val;
    }
}

void load_lua_mod_data( const sol::table &def, const std::string &member,
                        std::unordered_map<std::tuple<std::string, bool, std::string, std::string>, double,
                        cata::tuple_hash> &mod_data )
{
    sol::optional<sol::table> tbl = def[member];
    if( !tbl ) {
        return;
    }

    extract_lua_effect( *tbl, mod_data, "str_mod",          member, "STR",      "min" );
    extract_lua_effect( *tbl, mod_data, "dex_mod",          member, "DEX",      "min" );
    extract_lua_effect( *tbl, mod_data, "per_mod",          member, "PER",      "min" );
    extract_lua_effect( *tbl, mod_data, "int_mod",          member, "INT",      "min" );
    extract_lua_effect( *tbl, mod_data, "speed_mod",        member, "SPEED",    "min" );

    extract_lua_effect( *tbl, mod_data, "pain_amount",      member, "PAIN",     "amount" );
    extract_lua_effect( *tbl, mod_data, "pain_min",         member, "PAIN",     "min" );
    extract_lua_effect( *tbl, mod_data, "pain_max",         member, "PAIN",     "max" );
    extract_lua_effect( *tbl, mod_data, "pain_max_val",     member, "PAIN",     "max_val" );
    extract_lua_effect( *tbl, mod_data, "pain_chance",      member, "PAIN",     "chance_top" );
    extract_lua_effect( *tbl, mod_data, "pain_chance_bot",  member, "PAIN",     "chance_bot" );
    extract_lua_effect( *tbl, mod_data, "pain_tick",        member, "PAIN",     "tick" );

    extract_lua_effect( *tbl, mod_data, "hurt_amount",      member, "HURT",     "amount" );
    extract_lua_effect( *tbl, mod_data, "hurt_min",         member, "HURT",     "min" );
    extract_lua_effect( *tbl, mod_data, "hurt_max",         member, "HURT",     "max" );
    extract_lua_effect( *tbl, mod_data, "hurt_chance",      member, "HURT",     "chance_top" );
    extract_lua_effect( *tbl, mod_data, "hurt_chance_bot",  member, "HURT",     "chance_bot" );
    extract_lua_effect( *tbl, mod_data, "hurt_tick",        member, "HURT",     "tick" );

    extract_lua_effect( *tbl, mod_data, "sleep_amount",     member, "SLEEP",    "amount" );
    extract_lua_effect( *tbl, mod_data, "sleep_min",        member, "SLEEP",    "min" );
    extract_lua_effect( *tbl, mod_data, "sleep_max",        member, "SLEEP",    "max" );
    extract_lua_effect( *tbl, mod_data, "sleep_chance",     member, "SLEEP",    "chance_top" );
    extract_lua_effect( *tbl, mod_data, "sleep_chance_bot", member, "SLEEP",    "chance_bot" );
    extract_lua_effect( *tbl, mod_data, "sleep_tick",       member, "SLEEP",    "tick" );

    extract_lua_effect( *tbl, mod_data, "pkill_amount",     member, "PKILL",    "amount" );
    extract_lua_effect( *tbl, mod_data, "pkill_min",        member, "PKILL",    "min" );
    extract_lua_effect( *tbl, mod_data, "pkill_max",        member, "PKILL",    "max" );
    extract_lua_effect( *tbl, mod_data, "pkill_max_val",    member, "PKILL",    "max_val" );
    extract_lua_effect( *tbl, mod_data, "pkill_chance",     member, "PKILL",    "chance_top" );
    extract_lua_effect( *tbl, mod_data, "pkill_chance_bot", member, "PKILL",    "chance_bot" );
    extract_lua_effect( *tbl, mod_data, "pkill_tick",       member, "PKILL",    "tick" );

    extract_lua_effect( *tbl, mod_data, "stim_amount",      member, "STIM",     "amount" );
    extract_lua_effect( *tbl, mod_data, "stim_min",         member, "STIM",     "min" );
    extract_lua_effect( *tbl, mod_data, "stim_max",         member, "STIM",     "max" );
    extract_lua_effect( *tbl, mod_data, "stim_min_val",     member, "STIM",     "min_val" );
    extract_lua_effect( *tbl, mod_data, "stim_max_val",     member, "STIM",     "max_val" );
    extract_lua_effect( *tbl, mod_data, "stim_chance",      member, "STIM",     "chance_top" );
    extract_lua_effect( *tbl, mod_data, "stim_chance_bot",  member, "STIM",     "chance_bot" );
    extract_lua_effect( *tbl, mod_data, "stim_tick",        member, "STIM",     "tick" );

    extract_lua_effect( *tbl, mod_data, "health_amount",    member, "HEALTH",   "amount" );
    extract_lua_effect( *tbl, mod_data, "health_min",       member, "HEALTH",   "min" );
    extract_lua_effect( *tbl, mod_data, "health_max",       member, "HEALTH",   "max" );
    extract_lua_effect( *tbl, mod_data, "health_min_val",   member, "HEALTH",   "min_val" );
    extract_lua_effect( *tbl, mod_data, "health_max_val",   member, "HEALTH",   "max_val" );
    extract_lua_effect( *tbl, mod_data, "health_chance",    member, "HEALTH",   "chance_top" );
    extract_lua_effect( *tbl, mod_data, "health_chance_bot", member, "HEALTH",   "chance_bot" );
    extract_lua_effect( *tbl, mod_data, "health_tick",      member, "HEALTH",   "tick" );

    extract_lua_effect( *tbl, mod_data, "h_mod_amount",     member, "H_MOD",    "amount" );
    extract_lua_effect( *tbl, mod_data, "h_mod_min",        member, "H_MOD",    "min" );
    extract_lua_effect( *tbl, mod_data, "h_mod_max",        member, "H_MOD",    "max" );
    extract_lua_effect( *tbl, mod_data, "h_mod_min_val",    member, "H_MOD",    "min_val" );
    extract_lua_effect( *tbl, mod_data, "h_mod_max_val",    member, "H_MOD",    "max_val" );
    extract_lua_effect( *tbl, mod_data, "h_mod_chance",     member, "H_MOD",    "chance_top" );
    extract_lua_effect( *tbl, mod_data, "h_mod_chance_bot", member, "H_MOD",    "chance_bot" );
    extract_lua_effect( *tbl, mod_data, "h_mod_tick",       member, "H_MOD",    "tick" );

    extract_lua_effect( *tbl, mod_data, "rad_amount",       member, "RAD",      "amount" );
    extract_lua_effect( *tbl, mod_data, "rad_min",          member, "RAD",      "min" );
    extract_lua_effect( *tbl, mod_data, "rad_max",          member, "RAD",      "max" );
    extract_lua_effect( *tbl, mod_data, "rad_max_val",      member, "RAD",      "max_val" );
    extract_lua_effect( *tbl, mod_data, "rad_chance",       member, "RAD",      "chance_top" );
    extract_lua_effect( *tbl, mod_data, "rad_chance_bot",   member, "RAD",      "chance_bot" );
    extract_lua_effect( *tbl, mod_data, "rad_tick",         member, "RAD",      "tick" );

    extract_lua_effect( *tbl, mod_data, "hunger_amount",    member, "HUNGER",   "amount" );
    extract_lua_effect( *tbl, mod_data, "hunger_min",       member, "HUNGER",   "min" );
    extract_lua_effect( *tbl, mod_data, "hunger_max",       member, "HUNGER",   "max" );
    extract_lua_effect( *tbl, mod_data, "hunger_min_val",   member, "HUNGER",   "min_val" );
    extract_lua_effect( *tbl, mod_data, "hunger_max_val",   member, "HUNGER",   "max_val" );
    extract_lua_effect( *tbl, mod_data, "hunger_chance",    member, "HUNGER",   "chance_top" );
    extract_lua_effect( *tbl, mod_data, "hunger_chance_bot", member, "HUNGER",   "chance_bot" );
    extract_lua_effect( *tbl, mod_data, "hunger_tick",      member, "HUNGER",   "tick" );

    extract_lua_effect( *tbl, mod_data, "thirst_amount",    member, "THIRST",   "amount" );
    extract_lua_effect( *tbl, mod_data, "thirst_min",       member, "THIRST",   "min" );
    extract_lua_effect( *tbl, mod_data, "thirst_max",       member, "THIRST",   "max" );
    extract_lua_effect( *tbl, mod_data, "thirst_min_val",   member, "THIRST",   "min_val" );
    extract_lua_effect( *tbl, mod_data, "thirst_max_val",   member, "THIRST",   "max_val" );
    extract_lua_effect( *tbl, mod_data, "thirst_chance",    member, "THIRST",   "chance_top" );
    extract_lua_effect( *tbl, mod_data, "thirst_chance_bot", member, "THIRST",   "chance_bot" );
    extract_lua_effect( *tbl, mod_data, "thirst_tick",      member, "THIRST",   "tick" );

    extract_lua_effect( *tbl, mod_data, "fatigue_amount",    member, "FATIGUE",  "amount" );
    extract_lua_effect( *tbl, mod_data, "fatigue_min",       member, "FATIGUE",  "min" );
    extract_lua_effect( *tbl, mod_data, "fatigue_max",       member, "FATIGUE",  "max" );
    extract_lua_effect( *tbl, mod_data, "fatigue_min_val",   member, "FATIGUE",  "min_val" );
    extract_lua_effect( *tbl, mod_data, "fatigue_max_val",   member, "FATIGUE",  "max_val" );
    extract_lua_effect( *tbl, mod_data, "fatigue_chance",    member, "FATIGUE",  "chance_top" );
    extract_lua_effect( *tbl, mod_data, "fatigue_chance_bot", member, "FATIGUE",  "chance_bot" );
    extract_lua_effect( *tbl, mod_data, "fatigue_tick",      member, "FATIGUE",  "tick" );

    extract_lua_effect( *tbl, mod_data, "sleepdebt_amount",    member, "SLEEPDEBT", "amount" );
    extract_lua_effect( *tbl, mod_data, "sleepdebt_min",       member, "SLEEPDEBT", "min" );
    extract_lua_effect( *tbl, mod_data, "sleepdebt_max",       member, "SLEEPDEBT", "max" );
    extract_lua_effect( *tbl, mod_data, "sleepdebt_min_val",   member, "SLEEPDEBT", "min_val" );
    extract_lua_effect( *tbl, mod_data, "sleepdebt_max_val",   member, "SLEEPDEBT", "max_val" );
    extract_lua_effect( *tbl, mod_data, "sleepdebt_chance",    member, "SLEEPDEBT", "chance_top" );
    extract_lua_effect( *tbl, mod_data, "sleepdebt_chance_bot", member, "SLEEPDEBT", "chance_bot" );
    extract_lua_effect( *tbl, mod_data, "sleepdebt_tick",      member, "SLEEPDEBT", "tick" );

    extract_lua_effect( *tbl, mod_data, "stamina_amount",    member, "STAMINA",  "amount" );
    extract_lua_effect( *tbl, mod_data, "stamina_min",       member, "STAMINA",  "min" );
    extract_lua_effect( *tbl, mod_data, "stamina_max",       member, "STAMINA",  "max" );
    extract_lua_effect( *tbl, mod_data, "stamina_max_val",   member, "STAMINA",  "max_val" );
    extract_lua_effect( *tbl, mod_data, "stamina_chance",    member, "STAMINA",  "chance_top" );
    extract_lua_effect( *tbl, mod_data, "stamina_chance_bot", member, "STAMINA",  "chance_bot" );
    extract_lua_effect( *tbl, mod_data, "stamina_tick",      member, "STAMINA",  "tick" );

    extract_lua_effect( *tbl, mod_data, "cough_chance",     member, "COUGH",    "chance_top" );
    extract_lua_effect( *tbl, mod_data, "cough_chance_bot", member, "COUGH",    "chance_bot" );
    extract_lua_effect( *tbl, mod_data, "cough_tick",       member, "COUGH",    "tick" );

    extract_lua_effect( *tbl, mod_data, "vomit_chance",     member, "VOMIT",    "chance_top" );
    extract_lua_effect( *tbl, mod_data, "vomit_chance_bot", member, "VOMIT",    "chance_bot" );
    extract_lua_effect( *tbl, mod_data, "vomit_tick",       member, "VOMIT",    "tick" );

    extract_lua_effect( *tbl, mod_data, "healing_rate",    member, "HEAL_RATE",  "amount" );
    extract_lua_effect( *tbl, mod_data, "healing_head",    member, "HEAL_HEAD",  "amount" );
    extract_lua_effect( *tbl, mod_data, "healing_torso",   member, "HEAL_TORSO", "amount" );

    extract_lua_effect( *tbl, mod_data, "morale",          member, "MORALE",     "amount" );

    extract_lua_effect( *tbl, mod_data, "dodge_mod",    member, "DODGE",  "min" );
    extract_lua_effect( *tbl, mod_data, "hit_mod",      member, "HIT",    "min" );
    extract_lua_effect( *tbl, mod_data, "bash_mod",     member, "BASH",   "min" );
    extract_lua_effect( *tbl, mod_data, "cut_mod",      member, "CUT",    "min" );
    extract_lua_effect( *tbl, mod_data, "size_mod",     member, "SIZE",   "min" );
}

std::set<body_part> get_bodypart_set( const sol::table &def, const std::string &key )
{
    std::set<body_part> result;
    sol::optional<sol::table> tbl = def[key];
    if( !tbl ) {
        return result;
    }
    for( auto &pair : *tbl ) {
        if( pair.second.is<bodypart_str_id>() ) {
            result.insert( get_body_part_token( pair.second.as<bodypart_str_id>().str() ) );
        } else if( pair.second.is<std::string>() ) {
            result.insert( get_body_part_token( pair.second.as<std::string>() ) );
        }
    }
    return result;
}

std::map<body_part, int> get_bodypart_int_map_bp( const sol::table &def, const std::string &key )
{
    std::map<body_part, int> result;
    sol::optional<sol::table> tbl = def[key];
    if( !tbl ) {
        return result;
    }
    for( auto &pair : *tbl ) {
        std::string bp_str;
        if( pair.first.is<bodypart_str_id>() ) {
            bp_str = pair.first.as<bodypart_str_id>().str();
        } else if( pair.first.is<std::string>() ) {
            bp_str = pair.first.as<std::string>();
        } else {
            continue;
        }
        body_part bp = get_body_part_token( bp_str );
        result[bp] = pair.second.as<int>();
    }
    return result;
}

social_modifiers get_social_modifiers( const sol::table &def, const std::string &key )
{
    social_modifiers result;
    sol::optional<sol::table> tbl = def[key];
    if( !tbl ) {
        return result;
    }
    sol::object persuade_obj = ( *tbl )["persuade"];
    result.persuade = persuade_obj.valid() && persuade_obj.is<int>()
                      ? persuade_obj.as<int>() : 0;
    sol::object lie_obj = ( *tbl )["lie"];
    result.lie = lie_obj.valid() && lie_obj.is<int>()
                 ? lie_obj.as<int>() : 0;
    sol::object intimidate_obj = ( *tbl )["intimidate"];
    result.intimidate = intimidate_obj.valid() && intimidate_obj.is<int>()
                        ? intimidate_obj.as<int>() : 0;
    return result;
}

std::map<vitamin_id, time_duration> get_vitamin_rates( const sol::table &def,
        const std::string &key )
{
    std::map<vitamin_id, time_duration> result;
    sol::optional<sol::table> tbl = def[key];
    if( !tbl ) {
        return result;
    }
    for( auto &pair : *tbl ) {
        vitamin_id vit;
        if( pair.first.is<vitamin_id>() ) {
            vit = pair.first.as<vitamin_id>();
        } else if( pair.first.is<std::string>() ) {
            vit = vitamin_id( pair.first.as<std::string>() );
        } else {
            continue;
        }

        time_duration dur = 0_turns;
        if( pair.second.is<time_duration>() ) {
            dur = pair.second.as<time_duration>();
        } else if( pair.second.is<std::string>() ) {
            std::istringstream iss( pair.second.as<std::string>() );
            JsonIn jsin( iss );
            dur = read_from_json_string<time_duration>( jsin, time_duration::units );
        }
        result[vit] = dur;
    }
    return result;
}

std::map<body_part, resistances> get_mutation_armor( const sol::table &def, const std::string &key )
{
    std::map<body_part, resistances> result;
    sol::optional<sol::table> tbl = def[key];
    if( !tbl ) {
        return result;
    }
    for( auto &pair : *tbl ) {
        if( !pair.second.is<sol::table>() ) {
            continue;
        }
        sol::table entry = pair.second.as<sol::table>();

        std::vector<body_part> parts;
        sol::optional<sol::table> parts_tbl = entry["parts"];
        if( parts_tbl ) {
            for( auto &p : *parts_tbl ) {
                if( p.second.is<std::string>() ) {
                    std::string part_str = p.second.as<std::string>();
                    if( part_str == "ALL" ) {
                        for( int bp = 0; bp < static_cast<int>( num_bp ); ++bp ) {
                            parts.push_back( static_cast<body_part>( bp ) );
                        }
                    } else {
                        parts.push_back( get_body_part_token( part_str ) );
                    }
                }
            }
        }

        resistances res;
        if( entry["bash"].valid() ) {
            res.set_resist( DT_BASH, get_float( entry, "bash", 0.0f ) );
        }
        if( entry["cut"].valid() ) {
            res.set_resist( DT_CUT, get_float( entry, "cut", 0.0f ) );
        }
        if( entry["stab"].valid() ) {
            res.set_resist( DT_STAB, get_float( entry, "stab", 0.0f ) );
        }
        if( entry["bullet"].valid() ) {
            res.set_resist( DT_BULLET, get_float( entry, "bullet", 0.0f ) );
        }
        if( entry["acid"].valid() ) {
            res.set_resist( DT_ACID, get_float( entry, "acid", 0.0f ) );
        }
        if( entry["heat"].valid() ) {
            res.set_resist( DT_HEAT, get_float( entry, "heat", 0.0f ) );
        }
        if( entry["cold"].valid() ) {
            res.set_resist( DT_COLD, get_float( entry, "cold", 0.0f ) );
        }
        if( entry["electric"].valid() ) {
            res.set_resist( DT_ELECTRIC, get_float( entry, "electric", 0.0f ) );
        }
        if( entry["biological"].valid() ) {
            res.set_resist( DT_BIOLOGICAL, get_float( entry, "biological", 0.0f ) );
        }

        for( body_part bp : parts ) {
            result[bp] = res;
        }
    }
    return result;
}

/**
 * Conversion functions for each data type
 */
effect_rating parse_effect_rating( const std::string &rating )
{
    if( rating == "good" ) {
        return e_good;
    } else if( rating == "bad" ) {
        return e_bad;
    } else if( rating == "mixed" ) {
        return e_mixed;
    }
    return e_neutral;
}

} // namespace lua_data_helpers

using namespace lua_data_helpers;

/**
 * Convert Lua table to effect_type.
 * Uses LuaTableWrapper to leverage the unified DataReader abstraction.
 * Declared outside anonymous namespace so it can be friended by effect_type.
 */
effect_type lua_table_to_effect_type( const std::string &id, const sol::table &def )
{
    effect_type eff;
    LuaTableWrapper reader( def );

    // Handle copy_from - inherits from existing effect type
    if( reader.has_member( "copy_from" ) ) {
        std::string copy_from_str = reader.get_string( "copy_from" );
        efftype_id base_id( copy_from_str );
        if( base_id.is_valid() ) {
            eff = base_id.obj();
        } else {
            reader.throw_error( "copy_from target not found: " + copy_from_str, "copy_from" );
        }
    }

    // Mark as loaded if we copied from something (affects how optional handles defaults)
    const bool was_loaded = reader.has_member( "copy_from" );

    eff.id = efftype_id( id );

    eff.load_fields( reader, was_loaded );
    optional( reader, was_loaded, "name", eff.name );
    optional( reader, was_loaded, "desc", eff.desc );
    optional( reader, was_loaded, "reduced_desc", eff.reduced_desc );

    // Rating enum - custom handling
    if( reader.has_member( "rating" ) ) {
        std::string rating_str = reader.get_string( "rating" );
        eff.rating = parse_effect_rating( rating_str );
    }

    sol::optional<sol::table> resist_traits_tbl = def["resist_traits"];
    if( resist_traits_tbl ) {
        eff.resist_traits = get_string_id_array<mutation_branch>( def, "resist_traits" );
    }

    sol::optional<sol::table> resist_effects_tbl = def["resist_effects"];
    if( resist_effects_tbl ) {
        eff.resist_effects = get_string_id_array<effect_type>( def, "resist_effects" );
    }

    sol::optional<sol::table> removes_effects_tbl = def["removes_effects"];
    if( removes_effects_tbl ) {
        eff.removes_effects = get_string_id_array<effect_type>( def, "removes_effects" );
    }

    sol::optional<sol::table> blocks_effects_tbl = def["blocks_effects"];
    if( blocks_effects_tbl ) {
        eff.blocks_effects = get_string_id_array<effect_type>( def, "blocks_effects" );
    }

    // Flags - complex handling for string_id conversion
    sol::optional<sol::table> flags_tbl = def["flags"];
    if( flags_tbl ) {
        eff.flags.clear();
        for( auto &pair : *flags_tbl ) {
            if( pair.second.is<flag_id>() ) {
                eff.flags.insert( pair.second.as<flag_id>() );
            } else if( pair.second.is<std::string>() ) {
                eff.flags.insert( flag_id( pair.second.as<std::string>() ) );
            }
        }
    }

    // Mod data - complex structured data
    sol::optional<sol::table> base_mods = def["base_mods"];
    if( base_mods ) {
        load_lua_mod_data( def, "base_mods", eff.mod_data );
    }

    sol::optional<sol::table> scaling_mods = def["scaling_mods"];
    if( scaling_mods ) {
        load_lua_mod_data( def, "scaling_mods", eff.mod_data );
    }

    // Messages - complex structured data
    sol::optional<sol::table> miss_msgs_tbl = def["miss_messages"];
    if( miss_msgs_tbl ) {
        eff.miss_msgs = get_miss_messages( def, "miss_messages" );
    }

    sol::optional<sol::table> decay_msgs_tbl = def["decay_messages"];
    if( decay_msgs_tbl ) {
        eff.decay_msgs = get_decay_messages( def, "decay_messages" );
    }

    // Morale type
    sol::object morale_obj = def["morale"];
    if( morale_obj.valid() && !morale_obj.is<sol::lua_nil_t>() ) {
        if( morale_obj.is<morale_type>() ) {
            eff.morale = morale_obj.as<morale_type>();
        } else if( morale_obj.is<std::string>() ) {
            eff.morale = morale_type( morale_obj.as<std::string>() );
        }
    }

    // Effects on remove
    sol::optional<sol::table> effects_remove_tbl = def["effects_on_remove"];
    if( effects_remove_tbl ) {
        eff.effects_on_remove = get_effects_on_remove( def, "effects_on_remove" );
    }

    // Allow unvisited members (for forward compatibility and lua-specific fields)
    reader.allow_omitted_members();

    return eff;
}

mutation_branch lua_table_to_mutation(const std::string& id, const sol::table& def)
{
    mutation_branch mut;
    LuaTableWrapper reader(def);

    if (reader.has_member("copy_from")) {
        std::string copy_from_str = reader.get_string("copy_from");
        trait_id base_id(copy_from_str);
        if (base_id.is_valid()) {
            mut = base_id.obj();
        }
        else {
            reader.throw_error("copy_from target not found: " + copy_from_str, "copy_from");
        }
    }

    const bool was_loaded = reader.has_member("copy_from");
    mut.id = trait_id(id);
    mut.was_loaded = true;

    mut.load_fields(reader, was_loaded);

    // Complex structured data using Lua-specific helpers
    sol::optional<sol::table> armor = def["armor"];
    if (armor) {
        mut.armor = get_mutation_armor(def, "armor");
    }

    sol::optional<sol::table> lumination = def["lumination"];
    if (lumination) {
        mut.lumination = get_bodypart_float_map(def, "lumination");
    }

    sol::optional<sol::table> encumbrance_always = def["encumbrance_always"];
    if (encumbrance_always) {
        mut.encumbrance_always = get_bodypart_int_map_bp(def, "encumbrance_always");
    }

    sol::optional<sol::table> encumbrance_covered = def["encumbrance_covered"];
    if (encumbrance_covered) {
        mut.encumbrance_covered = get_bodypart_int_map_bp(def, "encumbrance_covered");
    }

    sol::optional<sol::table> restricts_gear = def["restricts_gear"];
    if (restricts_gear) {
        mut.restricts_gear = get_bodypart_set(def, "restricts_gear");
    }

    sol::optional<sol::table> allowed_items_tbl = def["allowed_items"];
    if (allowed_items_tbl) {
        mut.allowed_items = get_flag_id_set(def, "allowed_items");
    }

    // Item IDs (special handling for string/ID type flexibility)
    sol::object spawn_item = def["spawn_item"];
    if (spawn_item.valid() && !spawn_item.is<sol::lua_nil_t>()) {
        if (spawn_item.is<itype_id>()) {
            mut.spawn_item = spawn_item.as<itype_id>();
        }
        else if (spawn_item.is<std::string>()) {
            mut.spawn_item = itype_id(spawn_item.as<std::string>());
        }
    }

    sol::object ranged_mutation = def["ranged_mutation"];
    if (ranged_mutation.valid() && !ranged_mutation.is<sol::lua_nil_t>()) {
        if (ranged_mutation.is<itype_id>()) {
            mut.ranged_mutation = ranged_mutation.as<itype_id>();
        }
        else if (ranged_mutation.is<std::string>()) {
            mut.ranged_mutation = itype_id(ranged_mutation.as<std::string>());
        }
    }

    sol::optional<sol::table> vitamin_rates = def["vitamin_rates"];
    if (vitamin_rates) {
        mut.vitamin_rates = get_vitamin_rates(def, "vitamin_rates");
    }

    sol::optional<sol::table> spells_learned = def["spells_learned"];
    if (spells_learned) {
        mut.spells_learned = get_id_int_map<spell_type>(def, "spells_learned");
    }

    sol::optional<sol::table> craft_skill_bonus = def["craft_skill_bonus"];
    if (craft_skill_bonus) {
        mut.craft_skill_bonus = get_id_int_map<Skill>(def, "craft_skill_bonus");
    }

    sol::optional<sol::table> social_mods = def["social_modifiers"];
    if (social_mods) {
        mut.social_mods = get_social_modifiers(def, "social_modifiers");
    }

    // Allow unvisited members (for forward compatibility)
    reader.allow_omitted_members();

    return mut;
}

bionic_data lua_table_to_bionic(const std::string& id, const sol::table& def)
{
    bionic_data bio;
    LuaTableWrapper reader(def);

    // Handle copy_from
    if (reader.has_member("copy_from")) {
        std::string copy_from_str = reader.get_string("copy_from");
        bionic_id base_id(copy_from_str);
        if (base_id.is_valid()) {
            bio = base_id.obj();
        }
        else {
            reader.throw_error("copy_from target not found: " + copy_from_str, "copy_from");
        }
    }

    const bool was_loaded = reader.has_member("copy_from");
    bio.id = bionic_id(id);

    bio.load_fields( reader, was_loaded );

    bio.is_remote_fueled = bio.remote_fuel_draw > 0_J;

    sol::optional<sol::table> occupied = def["occupied_bodyparts"];
    if (occupied) {
        bio.occupied_bodyparts = get_bodypart_int_map(def, "occupied_bodyparts");
    }

    sol::optional<sol::table> encumbrance = def["encumbrance"];
    if (encumbrance) {
        bio.encumbrance = get_bodypart_int_map(def, "encumbrance");
    }

    sol::optional<sol::table> env_protec = def["env_protec"];
    if (env_protec) {
        bio.env_protec = get_bodypart_int_map(def, "env_protec");
    }

    sol::optional<sol::table> bash_protec = def["bash_protec"];
    if (bash_protec) {
        bio.bash_protec = get_bodypart_int_map(def, "bash_protec");
    }

    sol::optional<sol::table> cut_protec = def["cut_protec"];
    if (cut_protec) {
        bio.cut_protec = get_bodypart_int_map(def, "cut_protec");
    }

    sol::optional<sol::table> bullet_protec = def["bullet_protec"];
    if (bullet_protec) {
        bio.bullet_protec = get_bodypart_int_map(def, "bullet_protec");
    }

    sol::optional<sol::table> canceled_muts = def["canceled_mutations"];
    if (canceled_muts) {
        bio.canceled_mutations = get_string_id_array<mutation_branch>(def, "canceled_mutations");
    }

    sol::optional<sol::table> fuel_opts_tbl = def["fuel_options"];
    if (fuel_opts_tbl) {
        bio.fuel_opts.clear();
        for (auto& pair : *fuel_opts_tbl) {
            if (pair.second.is<itype_id>()) {
                bio.fuel_opts.push_back(pair.second.as<itype_id>());
            }
            else if (pair.second.is<std::string>()) {
                bio.fuel_opts.push_back(itype_id(pair.second.as<std::string>()));
            }
        }
    }

    sol::object fake_item = def["fake_item"];
    if (fake_item.valid() && !fake_item.is<sol::lua_nil_t>()) {
        if (fake_item.is<itype_id>()) {
            bio.fake_item = fake_item.as<itype_id>();
        }
        else if (fake_item.is<std::string>()) {
            bio.fake_item = itype_id(fake_item.as<std::string>());
        }
    }

    sol::optional<sol::table> stat_bonus = def["stat_bonus"];
    if (stat_bonus) {
        bio.stat_bonus = get_stat_bonus_map(def, "stat_bonus");
    }

    sol::optional<sol::table> enchantments = def["enchantments"];
    if (enchantments) {
        bio.enchantments = get_string_id_array<enchantment>(def, "enchantments");
    }

    sol::optional<sol::table> learned_spells = def["learned_spells"];
    if (learned_spells) {
        bio.learned_spells = get_id_int_map<spell_type>(def, "learned_spells");
    }

    sol::optional<sol::table> included_bionics = def["included_bionics"];
    if (included_bionics) {
        bio.included_bionics = get_string_id_array<bionic_data>(def, "included_bionics");
    }

    sol::object upgraded_bionic = def["upgraded_bionic"];
    if (upgraded_bionic.valid() && !upgraded_bionic.is<sol::lua_nil_t>()) {
        if (upgraded_bionic.is<bionic_id>()) {
            bio.upgraded_bionic = upgraded_bionic.as<bionic_id>();
        }
        else if (upgraded_bionic.is<std::string>()) {
            bio.upgraded_bionic = bionic_id(upgraded_bionic.as<std::string>());
        }
    }

    sol::optional<sol::table> available_upgrades = def["available_upgrades"];
    if (available_upgrades) {
        bio.available_upgrades = get_string_id_set<bionic_data>(def, "available_upgrades");
    }

    sol::optional<sol::table> required_bionics = def["required_bionics"];
    if (required_bionics) {
        bio.required_bionics = get_string_id_array<bionic_data>(def, "required_bionics");
    }

    sol::optional<sol::table> flags = def["flags"];
    if (flags) {
        bio.flags = get_flag_id_set(def, "flags");
    }

    sol::object power_gen_emission = def["power_gen_emission"];
    if (power_gen_emission.valid() && !power_gen_emission.is<sol::lua_nil_t>()) {
        if (power_gen_emission.is<emit_id>()) {
            bio.power_gen_emission = power_gen_emission.as<emit_id>();
        }
        else if (power_gen_emission.is<std::string>()) {
            bio.power_gen_emission = emit_id(power_gen_emission.as<std::string>());
        }
    }

    sol::object coverage_penalty = def["coverage_power_gen_penalty"];
    if (coverage_penalty.valid() && !coverage_penalty.is<sol::lua_nil_t>()) {
        bio.coverage_power_gen_penalty = get_optional_float(def, "coverage_power_gen_penalty");
    }

    // Computed activation state (same logic as JSON loading)
    bio.activated = bio.has_flag( flag_id( "BIONIC_TOGGLED" ) ) ||
                    bio.power_activate > 0_kJ ||
                    bio.charge_time > 0;

    // Allow unvisited members
    reader.allow_omitted_members();

    return bio;
}

namespace
{

using namespace lua_data_helpers;

void lua_value_to_json( std::ostringstream &ss, const sol::object &val );

void lua_table_to_json( std::ostringstream &ss, const sol::table &tbl )
{
    bool is_array = true;
    size_t expected_key = 1;
    for( auto &pair : tbl ) {
        if( !pair.first.is<int>() || pair.first.as<int>() != static_cast<int>( expected_key ) ) {
            is_array = false;
            break;
        }
        expected_key++;
    }

    if( is_array && expected_key > 1 ) {
        ss << "[";
        bool first = true;
        for( auto &pair : tbl ) {
            if( !first ) {
                ss << ",";
            }
            first = false;
            lua_value_to_json( ss, pair.second );
        }
        ss << "]";
    } else {
        ss << "{";
        bool first = true;
        for( auto &pair : tbl ) {
            if( !first ) {
                ss << ",";
            }
            first = false;

            if( pair.first.is<std::string>() ) {
                ss << "\"" << pair.first.as<std::string>() << "\"";
            } else if( pair.first.is<int>() ) {
                ss << "\"" << pair.first.as<int>() << "\"";
            } else {
                ss << "\"unknown\"";
            }
            ss << ":";

            lua_value_to_json( ss, pair.second );
        }
        ss << "}";
    }
}

void lua_value_to_json( std::ostringstream &ss, const sol::object &val )
{
    if( val.is<sol::lua_nil_t>() || !val.valid() ) {
        ss << "null";
    } else if( val.is<bool>() ) {
        ss << ( val.as<bool>() ? "true" : "false" );
    } else if( val.is<int>() ) {
        ss << val.as<int>();
    } else if( val.is<double>() ) {
        ss << val.as<double>();
    } else if( val.is<std::string>() ) {
        std::string s = val.as<std::string>();
        ss << "\"";
        for( char c : s ) {
            if( c == '"' ) {
                ss << "\\\"";
            } else if( c == '\\' ) {
                ss << "\\\\";
            } else if( c == '\n' ) {
                ss << "\\n";
            } else if( c == '\r' ) {
                ss << "\\r";
            } else if( c == '\t' ) {
                ss << "\\t";
            } else {
                ss << c;
            }
        }
        ss << "\"";
    } else if( val.is<sol::table>() ) {
        lua_table_to_json( ss, val.as<sol::table>() );
    } else {
        ss << "null";
    }
}

std::string lua_to_json_string( const sol::table &tbl )
{
    std::ostringstream ss;
    lua_table_to_json( ss, tbl );
    return ss.str();
}

/**
 * Convert Lua table to json_talk_topic.
 * Uses a Lua-to-JSON bridge to leverage existing JSON parsing logic.
 */
json_talk_topic lua_table_to_talk_topic( const std::string &/*id*/, const sol::table &def )
{
    json_talk_topic topic;

    std::string json_str = lua_to_json_string( def );
    std::istringstream iss( json_str );
    JsonIn jsin( iss );
    JsonObject jo = jsin.get_object();

    topic.load( jo );

    return topic;
}

/**
 * Process a single definition table, registering all entries.
 */
template<typename T, typename ConvertFunc, typename RegisterFunc>
void process_definition_table( sol::state &lua, const std::string &table_path,
                               ConvertFunc convert, RegisterFunc reg )
{
    sol::table tbl = lua["game"]["define"][table_path];
    for( auto it = tbl.begin(); it != tbl.end(); ++it ) {
        const auto ref = *it;
        std::string id;
        try {
            id = ref.first.as<std::string>();
            const sol::table def = ref.second.as<sol::table>();
            T obj = convert( id, def );
            reg( std::move( obj ) );
        } catch( const std::exception &e ) {
            debugmsg( "Failed to register Lua %s '%s': %s", table_path, id, e.what() );
        }
    }
}

} // anonymous namespace

namespace cata
{

void init_data_definition_tables( sol::state &lua )
{
    sol::table gt = lua.globals()["game"];

    sol::table define = lua.create_table();
    gt["define"] = define;

    // Preload-phase definition tables
    define["talk_topic"] = lua.create_table();
    define["effect_type"] = lua.create_table();
    define["mutation"] = lua.create_table();
    define["bionic"] = lua.create_table();

    // Finalize-phase definition tables (can reference JSON data)
    define["finalize_talk_topic"] = lua.create_table();
    define["finalize_effect_type"] = lua.create_table();
    define["finalize_mutation"] = lua.create_table();
    define["finalize_bionic"] = lua.create_table();
}

void reg_lua_data_definitions( lua_state &state )
{
    sol::state &lua = state.lua;

    process_definition_table<effect_type>(
        lua, "effect_type",
        lua_table_to_effect_type,
        register_lua_effect_type
    );

    {
        sol::table tbl = lua["game"]["define"]["talk_topic"];
        for( auto it = tbl.begin(); it != tbl.end(); ++it ) {
            const auto ref = *it;
            std::string id;
            try {
                id = ref.first.as<std::string>();
                const sol::table def = ref.second.as<sol::table>();
                json_talk_topic topic = lua_table_to_talk_topic( id, def );
                register_lua_talk_topic( id, std::move( topic ) );
            } catch( const std::exception &e ) {
                debugmsg( "Failed to register Lua talk_topic '%s': %s", id, e.what() );
            }
        }
    }

    process_definition_table<mutation_branch>(
        lua, "mutation",
        lua_table_to_mutation,
        mutation_branch::register_lua_mutation
    );

    process_definition_table<bionic_data>(
        lua, "bionic",
        lua_table_to_bionic,
        bionic_data::register_lua_bionic
    );
}

void reg_lua_finalize_definitions( lua_state &state )
{
    sol::state &lua = state.lua;

    process_definition_table<effect_type>(
        lua, "finalize_effect_type",
        lua_table_to_effect_type,
        register_lua_effect_type
    );

    {
        sol::table tbl = lua["game"]["define"]["finalize_talk_topic"];
        for( auto it = tbl.begin(); it != tbl.end(); ++it ) {
            const auto ref = *it;
            std::string id;
            try {
                id = ref.first.as<std::string>();
                const sol::table def = ref.second.as<sol::table>();
                json_talk_topic topic = lua_table_to_talk_topic( id, def );
                register_lua_talk_topic( id, std::move( topic ) );
            } catch( const std::exception &e ) {
                debugmsg( "Failed to register Lua talk_topic '%s': %s", id, e.what() );
            }
        }
    }

    process_definition_table<mutation_branch>(
        lua, "finalize_mutation",
        lua_table_to_mutation,
        mutation_branch::register_lua_mutation
    );

    process_definition_table<bionic_data>(
        lua, "finalize_bionic",
        lua_table_to_bionic,
        bionic_data::register_lua_bionic
    );
}

} // namespace cata
