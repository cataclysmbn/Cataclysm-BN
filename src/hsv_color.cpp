#include "hsv_color.h"

#include <algorithm>
#include <sstream>

#include "rng.h"
#include "translations.h"

#if defined(TILES)
#include "sdl_utils.h"
#else
#include "ncurses_def.h"
#endif

static std::unordered_map<RGBColor, std::string> named_colors = {};
static std::unordered_map<RGBColor, std::string> similar_name_cache = {};

void RGBColor::load_named_color( const JsonObject &jo, const std::string & )
{
    const auto name =    jo.get_string( "name" );
    if( jo.has_member( "value" ) ) {
        const auto value_obj =    jo.get_raw( "value" );
        RGBColor color;
        color.deserialize( *value_obj );

        named_colors.insert_or_assign( color, name );
    }
}

void RGBColor::unload_names()
{
    named_colors.clear();
    similar_name_cache.clear();
}

static auto char_cmp_ignore_case( const char a, const char b )
{
    return std::tolower( static_cast<unsigned char>( a ) )
           == std::tolower( static_cast<unsigned char>( b ) );
}

std::pair<RGBColor, std::string> RGBColor::random_named( std::string fuzzy_match )
{
    if( fuzzy_match.empty() ) {
        return random_entry( named_colors );
    }

    std::vector<decltype( named_colors )::value_type> candidates;
    std::ranges::copy_if( named_colors, std::back_inserter( candidates ), [&]( const auto & c ) {
        auto it = std::search( c.second.begin(), c.second.end(), fuzzy_match.begin(), fuzzy_match.end(),
                               char_cmp_ignore_case );
        return it != c.second.end();
    } );
    return random_entry( candidates );
}

std::unordered_map<RGBColor, std::string> RGBColor::get_all_named_colors()
{
    return named_colors;
};

std::string RGBColor::friendly_name() const
{
    const auto it = named_colors.find( *this );
    if( it != named_colors.end() ) {
        return it->second;
    }

    // https://www.compuphase.com/cmetric.htm
    const auto distFunc = []( const RGBColor & e1, const RGBColor & e2 ) {
        const auto r_mean = ( static_cast<uint32_t>( e1.r ) + static_cast<uint32_t>( e2.r ) ) / 2;
        const auto r = static_cast<uint32_t>( e1.r ) - static_cast<uint32_t>( e2.r );
        const auto g = static_cast<uint32_t>( e1.g ) - static_cast<uint32_t>( e2.g );
        const auto b = static_cast<uint32_t>( e1.b ) - static_cast<uint32_t>( e2.b );
        return sqrt( ( ( ( 512 + r_mean ) * r * r ) >> 8 )
                     + 4 * g * g
                     + ( ( ( 767 - r_mean ) * b * b ) >> 8 ) );
    };

    const auto nearest = similar_name_cache.find( *this );
    if( nearest != similar_name_cache.end() ) {
        return nearest->second;
    }

    const auto min = std::ranges::min_element( named_colors,
    [&]( const RGBColor & c1,    const RGBColor & c2 ) {
        const auto da = distFunc( c1, *this );
        const auto db = distFunc( c2, *this );
        return  da < db;
    }, []( const auto & i ) { return i.first; } );
    auto similar_name = string_format( _( "%s (Off-Brand)" ), min->second );
    similar_name_cache.emplace( *this, similar_name );
    return similar_name;
}

auto curses_color_to_RGB( const nc_color &color ) -> RGBColor
{
#if defined(TILES)
    return curses_color_to_SDL( color );
#else
    return ncurses::color_to_RGB( color );;
#endif
}

static auto median( const uint8_t a, const uint8_t b, const uint8_t c )
{
    if( ( a > b ) ^ ( a > c ) ) {
        return a;
    }
    if( ( b < a ) ^ ( b < c ) ) {
        return b;
    }
    return c;
};

auto hsv2rgb( HSVColor color ) -> RGBColor
{
    constexpr auto E = ( 1 << 16 ) - 1;

    const auto [H, S, V, A] = color;

    if( S == 0 || V == 0 ) {
        return RGBColor{V, V, V, A};
    }

    uint8_t I;
    if( H < E ) {
        I = 0;
    } else if( H < 2 * E ) {
        I = 1;
    } else if( H < 3 * E ) {
        I = 2;
    } else if( H < 4 * E ) {
        I = 3;
    } else if( H < 5 * E ) {
        I = 4;
    } else {
        I = 5;
    }

    auto F = ( H - ( E * I ) );
    if( F == 0 ) {
        ++F;
    }

    if( I % 2 != 0 ) {
        F = ( E - F );
    }

    const auto d = ( ( S * V ) >> 16 ) + 1;
    const auto m = static_cast<uint8_t>( V - d );
    const auto c = static_cast<uint8_t>( ( ( F * d ) >> 16 ) + m );

    switch( I ) {
        case 0:
            return {V, c, m, A};
        case 1:
            return {c, V, m, A};
        case 2:
            return {m, V, c, A};
        case 3:
            return {m, c, V, A};
        case 4:
            return {c, m, V, A};
        case 5:
            return {V, m, c, A};
        default:
            return {0, 0, 0, A};
    }
}

auto rgb2hsv( RGBColor color ) -> HSVColor
{
    const auto [R, G, B, A] = color;
    const auto min = std::min( {R, G, B } );
    const auto max = std::max( {R, G, B } );
    const auto med = median( R, G, B );

    const auto V = max;

    const auto d = max - min;
    if( d == 0 || max == 0 ) {
        return HSVColor{0, 0, V, A};
    }

    const auto S = static_cast<uint16_t>( ( ( d << 16 ) - 1 ) / V );

    int I;
    if( max == R && min == B ) {
        I = 0;
    } else if( max == G && min == B ) {
        I = 1;
    } else if( max == G && min == R ) {
        I = 2;
    } else if( max == B && min == R ) {
        I = 3;
    } else if( max == B && min == G ) {
        I = 4;
    } else {
        I = 5;
    }

    constexpr auto E = ( 1 << 16 ) - 1;
    auto F = ( ( ( med - min ) << 16 ) / d ) + 1;
    if( I % 2 != 0 ) {
        F = E - F;
    }

    const auto H = static_cast<uint32_t>( ( E * I ) + F );

    return HSVColor{H, S, V, A};
}

void RGBColor::serialize( JsonOut &jsout ) const
{
    jsout.start_array();
    jsout.write( r );
    jsout.write( g );
    jsout.write( b );
    if( a != 255 ) {
        jsout.write( a );
    }
    jsout.end_array();
}

void RGBColor::deserialize( JsonIn &jsin )
{
    if( jsin.test_array() ) {
        const auto arr = jsin.get_array();
        if( arr.size() == 3 ) {
            r = static_cast<uint8_t>( std::clamp( arr.get_int( 0 ), 0, 255 ) );
            g = static_cast<uint8_t>( std::clamp( arr.get_int( 1 ), 0, 255 ) );
            b = static_cast<uint8_t>( std::clamp( arr.get_int( 2 ), 0, 255 ) );
            a = 255;
        } else if( arr.size() == 4 ) {
            r = static_cast<uint8_t>( std::clamp( arr.get_int( 0 ), 0, 255 ) );
            g = static_cast<uint8_t>( std::clamp( arr.get_int( 1 ), 0, 255 ) );
            b = static_cast<uint8_t>( std::clamp( arr.get_int( 2 ), 0, 255 ) );
            a = static_cast<uint8_t>( std::clamp( arr.get_int( 3 ), 0, 255 ) );
        } else {
            jsin.error( "Invalid color value" );
        }
    } else if( jsin.test_string() ) {
        const auto str = jsin.get_string();
        const auto col = try_parse( str );
        if( col.has_value() ) {
            *this = col.value();
        } else {
            debugmsg( "Unknown color value: %s", str.c_str() );
        }
    } else {
        debugmsg( "Unknown color value" );
    }
}

void RGBColorPair::serialize( JsonOut &jsout ) const
{
    if( fg == bg ) {
        jsout.write( fg );
    } else {
        jsout.start_array();
        jsout.write( fg );
        jsout.write( bg );
        jsout.end_array();
    }
}

void RGBColorPair::deserialize( JsonIn &jsin )
{
    if( jsin.test_string() ) {
        jsin.read( fg );
        bg = fg;
    } else if( jsin.test_array() ) {
        const auto pos = jsin.tell();

        const auto tmp = jsin.get_array();
        if( tmp.size() == 2 ) {
            tmp.read( 0, fg );
            tmp.read( 1, bg );
        } else {
            jsin.seek( pos );
            jsin.read( fg );
            bg = fg;
        }
    } else {
        debugmsg( "Invalid color pair value" );
    }
}

static RGBColor rgb_from_hex_string( std::string str )
{
    if( str.starts_with( "#" ) ) {
        str = str.substr( 1 );
    }

    if( str.empty() || std::ranges::any_of( str, []( const char &c ) { return !std::isxdigit( c ); } ) ) {
        debugmsg( "Invalid color value: %s", str );
        return {};
    }

    uint32_t d;
    std::istringstream is( str );
    is >> std::hex >> d;
    switch( str.size() ) {
        case 3: {
            const auto nr = static_cast<uint8_t>( ( d >> 8 ) & 0x0F );
            const auto ng = static_cast<uint8_t>( ( d >> 4 ) & 0x0F );
            const auto nb = static_cast<uint8_t>( ( d >> 0 ) & 0x0F );
            return RGBColor{static_cast<uint8_t>( nr | nr << 4 ),
                            static_cast<uint8_t>( ng | ng << 4 ),
                            static_cast<uint8_t>( nb | nb << 4 ),
                            static_cast<uint8_t>( 255 )};
        }
        case 4: {
            const auto nr = static_cast<uint8_t>( ( d >> 12 ) & 0x0F );
            const auto ng = static_cast<uint8_t>( ( d >> 8 ) & 0x0F );
            const auto nb = static_cast<uint8_t>( ( d >> 4 ) & 0x0F );
            const auto na = static_cast<uint8_t>( ( d >> 0 ) & 0x0F );
            return RGBColor{
                static_cast<uint8_t>( nr | nr << 4 ),
                static_cast<uint8_t>( ng | ng << 4 ),
                static_cast<uint8_t>( nb | nb << 4 ),
                static_cast<uint8_t>( na | na << 4 ),
            };
        }
        case 6: {
            return RGBColor{
                static_cast<uint8_t>( d >> 16 ), static_cast<uint8_t>( d >> 8 ),
                static_cast<uint8_t>( d >> 0 ), static_cast<uint8_t>( 255 )};
        }
        case 8: {
            return RGBColor{
                static_cast<uint8_t>( d >> 24 ),
                static_cast<uint8_t>( d >> 16 ),
                static_cast<uint8_t>( d >> 8 ),
                static_cast<uint8_t>( d >> 0 ),
            };
        }
        default:
            debugmsg( "Invalid color value: %", str );
            return {};
    }
}

std::optional<RGBColor> RGBColor::try_parse( const std::string &str )
{
    if( str.starts_with( "#" ) ) {
        return rgb_from_hex_string( str );
    }

    if( str.starts_with( "!" ) ) {
        return random_named( str.substr( 1 ) ).first;
    }

    const auto &cm = get_all_colors();
    const auto nc_id = cm.name_to_id( str, report_color_error::no );
    if( nc_id != def_c_unset ) {
        return curses_color_to_RGB( cm.get( nc_id ) );
    }

    for( const auto & [color, name] : named_colors ) {
        if( std::ranges::equal( str, name, char_cmp_ignore_case ) ) {
            return color;
        }
    }

    return std::nullopt;
}

bool detail::RGBColorConverter::operator()( const std::string &str,
        RGBColor &col ) const
{
    const auto c = RGBColor::try_parse( str );
    if( !c.has_value() ) {
        return false;
    }
    col = c.value();
    return true;
}

bool detail::RGBColorConverter::operator()( const RGBColor &col,
        std::string &str ) const
{
    str = string_format( "#%02x%02x%02x%02x", col.r, col.g, col.b, col.a );
    return true;
}

bool detail::RGBColorConverter::should_cache( const std::string &s, const RGBColor & )
{
    return !s.starts_with( "!" );
}
