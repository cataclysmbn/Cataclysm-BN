#include "wheel_dimensions.h"

#include <cmath>
#include <locale>
#include <sstream>
#include <string>

#include "json.h"
#include "options.h"
#include "string_formatter.h"
#include "translations.h"

namespace wheel_dimensions
{

namespace
{

auto parse_metric_dimension( const JsonObject &jo, const std::string &member ) -> int
{
    auto value = 0;
    auto suffix = std::string{};

    std::istringstream stream( jo.get_string( member ) );
    stream.imbue( std::locale::classic() );
    stream >> value >> suffix;

    if( !stream || stream.peek() != std::istringstream::traits_type::eof() ) {
        jo.throw_error( "syntax error when specifying wheel dimensions", member );
    }

    if( value < 0 ) {
        jo.throw_error( "wheel dimensions must not be negative", member );
    }

    if( suffix != "mm" ) {
        jo.throw_error( "wheel dimensions must use the mm suffix", member );
    }

    return value;
}

auto format_metric_dimension( const int millimeters ) -> std::string
{
    if( millimeters % 10 == 0 ) {
        return string_format( pgettext( "wheel dimension", "%dcm" ), millimeters / 10 );
    }

    const auto centimeters = static_cast<double>( millimeters ) / 10.0;
    return string_format( pgettext( "wheel dimension", "%.1fcm" ), centimeters );
}

} // namespace

auto from_legacy_inches( const int legacy_inches ) -> int
{
    return legacy_inches * legacy_millimeters_per_inch;
}

auto read_from_json( const JsonObject &jo, const std::string &member ) -> std::optional<int>
{
    if( !jo.has_member( member ) ) {
        return std::nullopt;
    }

    if( jo.has_int( member ) ) {
        return from_legacy_inches( jo.get_int( member ) );
    }

    if( jo.has_string( member ) ) {
        return parse_metric_dimension( jo, member );
    }

    jo.throw_error( "wheel dimensions must be an integer legacy inch value or a \"<n> mm\" string",
                    member );
}

auto format_for_display( const int millimeters ) -> std::string
{
    if( get_option<std::string>( "DISTANCE_UNITS" ) == "imperial" ) {
        const auto inches = static_cast<int>( std::lround( static_cast<double>( millimeters ) /
                                              legacy_millimeters_per_inch ) );
        return string_format( pgettext( "wheel dimension", "%d\"" ), inches );
    }

    return format_metric_dimension( millimeters );
}

} // namespace wheel_dimensions
