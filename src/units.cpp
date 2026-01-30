#include "json.h"
#include "string_formatter.h"
#include "units.h"
#include "units_serde.h"
#include "translations.h"

namespace units
{
template<>
void volume::serialize( JsonOut &jsout ) const
{
    if( value_ % 1000 == 0 ) {
        jsout.write( string_format( "%d L", value_ / 1000 ) );
    } else {
        jsout.write( string_format( "%d ml", value_ ) );
    }
}

template<>
void volume::deserialize( JsonIn &jsin )
{
    *this = read_from_json_string( jsin, units::volume_units );
}

template<>
void mass::serialize( JsonOut &jsout ) const
{
    if( value_ % 1000000 == 0 ) {
        jsout.write( string_format( "%d kg", value_ / 1000000 ) );
    } else if( value_ % 1000 == 0 ) {
        jsout.write( string_format( "%d g", value_ / 1000 ) );
    } else {
        jsout.write( string_format( "%d mg", value_ ) );
    }
}

template<>
void mass::deserialize( JsonIn &jsin )
{
    *this = read_from_json_string( jsin, units::mass_units );
}

template<>
void energy::serialize( JsonOut &jsout ) const
{
    if( value_ % 1000 == 0 ) {
        jsout.write( string_format( "%d kJ", value_ / 1000 ) );
    } else {
        jsout.write( string_format( "%d J", value_ ) ) ;
    }
}

template<>
void energy::deserialize( JsonIn &jsin )
{
    *this = read_from_json_string( jsin, units::energy_units );
}

template<>
void angle::serialize( JsonOut &jsout ) const
{
    jsout.write( string_format( "%f rad", value_ ) );
}

template<>
void angle::deserialize( JsonIn &jsin )
{
    *this = read_from_json_string( jsin, units::angle_units );
}

std::string display( const units::energy v )
{
    const int kj = units::to_kilojoule( v );
    const int j = units::to_joule( v );
    // at least 1 kJ and there is no fraction
    if( kj >= 1 && float( j ) / kj == 1000 ) {
        return std::to_string( kj ) + ' ' + pgettext( "energy unit: kilojoule", "kJ" );
    }
    return std::to_string( j ) + ' ' + pgettext( "energy unit: joule", "J" );
}
} // namespace units
