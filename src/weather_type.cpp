#include "weather_type.h"

#include "units_serde.h"
#include "game_constants.h"
#include "generic_factory.h"
#include "bodypart.h"
#include "weather.h"

namespace
{
generic_factory<weather_type> weather_type_factory( "weather_type" );
} // namespace


namespace io
{
template<>
std::string enum_to_string<precip_class>( precip_class data )
{
    switch( data ) {
        case precip_class::none:
            return "none";
        case precip_class::very_light:
            return "very_light";
        case precip_class::light:
            return "light";
        case precip_class::medium:
            return "medium";
        case precip_class::heavy:
            return "heavy";
        case precip_class::last:
            break;
    }
    debugmsg( "Invalid precip_class" );
    abort();
}

template<>
std::string enum_to_string<sun_intensity_type>( sun_intensity_type data )
{
    switch( data ) {
        case sun_intensity_type::none:
            return "none";
        case sun_intensity_type::light:
            return "light";
        case sun_intensity_type::normal:
            return "normal";
        case sun_intensity_type::high:
            return "high";
        case sun_intensity_type::last:
            break;
    }
    debugmsg( "Invalid sun_intensity_type" );
    abort();
}

template<>
std::string enum_to_string<weather_time_requirement_type>( weather_time_requirement_type data )
{
    switch( data ) {
        case weather_time_requirement_type::day:
            return "day";
        case weather_time_requirement_type::night:
            return "night";
        case weather_time_requirement_type::both:
            return "both";
        case weather_time_requirement_type::last:
            break;
    }
    debugmsg( "Invalid time_requirement_type" );
    abort();
}

template<>
std::string enum_to_string<weather_sound_category>( weather_sound_category data )
{
    switch( data ) {
        case weather_sound_category::drizzle:
            return "drizzle";
        case weather_sound_category::flurries:
            return "flurries";
        case weather_sound_category::rainy:
            return "rainy";
        case weather_sound_category::snow:
            return "snow";
        case weather_sound_category::snowstorm:
            return "snowstorm";
        case weather_sound_category::thunder:
            return "thunder";
        case weather_sound_category::silent:
            return "silent";
        case weather_sound_category::last:
            break;
    }
    debugmsg( "Invalid time_requirement_type" );
    abort();
}

} // namespace io

template<>
const weather_type &weather_type_id::obj() const
{
    return weather_type_factory.obj( *this );
}

/** @relates string_id */
template<>
bool string_id<weather_type>::is_valid() const
{
    return weather_type_factory.is_valid( *this );
}

void weather_type::check() const
{
    for( const weather_type_id &required : requirements.required_weathers ) {
        if( !required.is_valid() ) {
            // This may be important, throw error and abort loading.
            throw string_format( R"(Weather type "%s" required for weather type "%s" does not exist.)",
                                 required, id );
        }
    }
}

void weather_type::load( const JsonObject &jo, const std::string & )
{
    mandatory( jo, was_loaded, "name", name );
    mandatory( jo, was_loaded, "id",  id );

    optional( jo, was_loaded, "color", color );
    optional( jo, was_loaded, "map_color", map_color );
    mandatory( jo, was_loaded, "glyph", symbol, unicode_codepoint_from_symbol_reader );

    mandatory( jo, was_loaded, "ranged_penalty", ranged_penalty );
    mandatory( jo, was_loaded, "sight_penalty", sight_penalty );
    mandatory( jo, was_loaded, "light_modifier", light_modifier );

    mandatory( jo, was_loaded, "sound_attn", sound_attn );
    mandatory( jo, was_loaded, "dangerous", dangerous );
    mandatory( jo, was_loaded, "precip", precip );
    mandatory( jo, was_loaded, "rains", rains );
    optional( jo, was_loaded, "acidic", acidic, false );
    optional( jo, was_loaded, "sound_category", sound_category, weather_sound_category::silent );
    mandatory( jo, was_loaded, "sun_intensity", sun_intensity );

    for( const JsonObject weather_effect : jo.get_array( "effects" ) ) {
        std::string name = weather_effect.get_string( "name" );
        int intensity = weather_effect.get_int( "intensity" );

        // this is a terrible hardcoded implementation, but only way i could figure out how to satisfy JSON and get it to function
        if( name == "morale" ) {
            std::string id_str = weather_effect.get_string( "morale_id_str" );
            std::string msg = weather_effect.get_string( "morale_msg" );
            int freq = weather_effect.get_int( "morale_msg_frequency" );
            int bonus = weather_effect.get_int( "bonus" );
            int bonus_max = weather_effect.get_int( "bonus_max" );
            time_duration duration = read_from_json_string<time_duration>
                                     ( *weather_effect.get_raw( "duration" ),
                                       time_duration::units );
            time_duration decay_start = read_from_json_string<time_duration>
                                        ( *weather_effect.get_raw( "decay_start" ),
                                          time_duration::units );
            int message_type = weather_effect.get_int( "message_type" );
            game_message_type gmt = static_cast<game_message_type>( message_type );

            effects.emplace_back(
            [ = ]( int intensity ) {
                weather_effect::morale( intensity, bonus, bonus_max, duration, decay_start, id_str, msg, freq,
                                        gmt );
            },
            intensity
            );
            continue; // skip the map lookup
        }

        // same as above
        if( name == "effect" ) {
            std::string id_str = weather_effect.get_string( "effect_id_str" );
            std::string msg = weather_effect.get_string( "effect_msg" );
            int freq = weather_effect.get_int( "effect_msg_frequency" );
            int blocked_freq = weather_effect.get_int( "effect_msg_blocked_frequency" );
            int effect_intensity = weather_effect.get_int( "effect_intensity" );
            std::string bodypart_string = weather_effect.get_string( "bodypart_string", "" );
            time_duration duration = read_from_json_string<time_duration>
                                     ( *weather_effect.get_raw( "duration" ),
                                       time_duration::units );

            bodypart_str_id bp_id = bodypart_str_id::NULL_ID();
            if( !bodypart_string.empty() ) {
                bp_id = bodypart_str_id( bodypart_string );
            }

            std::string precipitation_name = weather_effect.get_string( "precipitation_name" );
            bool ignore_armor = weather_effect.get_bool( "ignore_armor" );
            int message_type = weather_effect.get_int( "message_type" );
            int clothing_protection = weather_effect.get_int( "clothing_protection" );
            int umbrella_protection = weather_effect.get_int( "umbrella_protection" );
            game_message_type gmt = static_cast<game_message_type>( message_type );

            effects.emplace_back(
            [ = ]( int intensity ) {
                weather_effect::effect( intensity, duration, bp_id, effect_intensity, id_str, msg,
                                        freq, blocked_freq,
                                        gmt, precipitation_name, ignore_armor, clothing_protection, umbrella_protection );
            },
            intensity
            );
            continue; // skip the map lookup
        }

        const std::map<std::string, weather_effect_fn> all_weather_effects = {
            { "wet", &weather_effect::wet_player },
            { "thunder", &weather_effect::thunder },
            { "lightning", &weather_effect::lightning }
            // effect and morale would be here, but are hardcoded above
        };

        const auto iter = all_weather_effects.find( name );
        if( iter == all_weather_effects.end() ) {
            weather_effect.throw_error( "Invalid weather effect", "name" );
        }

        effects.emplace_back( iter->second, intensity );
    }

    if( jo.has_member( "animation" ) ) {
        animation = {};
        JsonObject j = jo.get_object( "animation" );

        mandatory( j, was_loaded, "factor", animation.factor );
        mandatory( j, was_loaded, "tile", animation.tile );
        mandatory( j, was_loaded, "color", animation.color );
        mandatory( j, was_loaded, "glyph", animation.symbol, unicode_codepoint_from_symbol_reader );
    }

    if( jo.has_member( "requirements" ) ) {
        requirements = {};
        JsonObject j = jo.get_object( "requirements" );

        optional( j, was_loaded, "pressure_min", requirements.pressure_min, INT_MIN );
        optional( j, was_loaded, "pressure_max", requirements.pressure_max, INT_MAX );
        optional( j, was_loaded, "humidity_min", requirements.humidity_min, INT_MIN );
        optional( j, was_loaded, "humidity_max", requirements.humidity_max, INT_MAX );
        optional( j, was_loaded, "temperature_min", requirements.temperature_min, INT_MIN );
        optional( j, was_loaded, "temperature_max", requirements.temperature_max, INT_MAX );
        optional( j, was_loaded, "windpower_min", requirements.windpower_min, INT_MIN );
        optional( j, was_loaded, "windpower_max", requirements.windpower_max, INT_MAX );
        optional( j, was_loaded, "humidity_and_pressure", requirements.humidity_and_pressure, true );
        optional( j, was_loaded, "acidic", requirements.acidic, false );
        optional( j, was_loaded, "time", requirements.time, weather_time_requirement_type::both );
        optional( j, was_loaded, "required_weathers", requirements.required_weathers );
    }
}

void weather_types::reset()
{
    weather_type_factory.reset();
}

void weather_types::finalize_all()
{
    weather_type_factory.finalize();
}

const std::vector<weather_type> &weather_types::get_all()
{
    return weather_type_factory.get_all();
}

void weather_types::check_consistency()
{
    weather_type_factory.check();
}

void weather_types::load( const JsonObject &jo, const std::string &src )
{
    weather_type_factory.load( jo, src );
}

