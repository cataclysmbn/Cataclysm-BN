#include "om_direction.h" // IWYU pragma: associated
#include "cube_direction.h" // IWYU pragma: associated
#include "enum_conversions.h"
#include "omdata.h" // IWYU pragma: associated
#include "overmap_special.h" // IWYU pragma: associated
#include "overmap.h" // IWYU pragma: associated

#include <algorithm>
#include <cassert>
#include <cmath>
#include <coordinates.h>
#include <cstddef>
#include <cstring>
#include <exception>
#include <memory>
#include <numeric>
#include <optional>
#include <ostream>
#include <point.h>
#include <set>
#include <submap.h>
#include <tuple>
#include <unordered_set>
#include <vector>
#include <vehicle.h>

#include "all_enum_values.h"
#include "assign.h"
#include "cata_utility.h"
#include "catacharset.h"
#include "character_id.h"
#include "coordinate_conversions.h"
#include "debug.h"
#include "distribution.h"
#include "flood_fill.h"
#include "fstream_utils.h"
#include "game.h"
#include "generic_factory.h"
#include "json.h"
#include "line.h"
#include "map.h"
#include "map_iterator.h"
#include "mapbuffer.h"
#include "mapgen.h"
#include "mapgen_functions.h"
#include "math_defines.h"
#include "messages.h"
#include "mongroup.h"
#include "monster.h"
#include "mtype.h"
#include "name.h"
#include "npc.h"
#include "options.h"
#include "output.h"
#include "overmap_connection.h"
#include "overmap_location.h"
#include "overmap_noise.h"
#include "overmap_types.h"
#include "overmapbuffer.h"
#include "regional_settings.h"
#include "rng.h"
#include "rotatable_symbols.h"
#include "sets_intersect.h"
#include "simple_pathfinding.h"
#include "string_formatter.h"
#include "string_utils.h"
#include "text_snippets.h"
#include "translations.h"
#include "type_id.h"
#include "weighted_list.h"
#include "world.h"

static const efftype_id effect_pet( "pet" );

static const species_id ZOMBIE( "ZOMBIE" );

static const mongroup_id GROUP_DIMENSIONAL_SURFACE( "GROUP_DIMENSIONAL_SURFACE" );
static const mongroup_id GROUP_WORM( "GROUP_WORM" );
static const mongroup_id GROUP_ZOMBIE( "GROUP_ZOMBIE" );
static const mongroup_id GROUP_NEMESIS( "GROUP_NEMESIS" );

static const oter_type_str_id oter_type_bridge( "bridge" );

class map_extra;

#define dbg(x) DebugLogFL((x),DC::MapGen)

enum {
    BUILDINGCHANCE = 4
};

////////////////
oter_id  ot_null,
         ot_crater,
         ot_field,
         ot_forest,
         ot_forest_thick,
         ot_forest_water,
         ot_river_center;

const oter_type_t oter_type_t::null_type{};

namespace io
{

template<>
std::string enum_to_string<overmap_special_subtype>( overmap_special_subtype data )
{
    switch( data ) {
        // *INDENT-OFF*
        case overmap_special_subtype::fixed: return "fixed";
        case overmap_special_subtype::mutable_: return "mutable";
        // *INDENT-ON*
        case overmap_special_subtype::last:
            break;
    }
    debugmsg( "Invalid overmap_special_subtype" );
    abort();
}

template<>
std::string enum_to_string<om_direction::type>( om_direction::type d )
{
    switch( d ) {
        // *INDENT-OFF*
        case om_direction::type::north: return "north";
        case om_direction::type::east: return "east";
        case om_direction::type::south: return "south";
        case om_direction::type::west: return "west";
        // *INDENT-ON*
        case om_direction::type::invalid:
        case om_direction::type::last:
            break;
    }
    debugmsg( "Invalid om_direction" );
    abort();
}

template<>
std::string enum_to_string<cube_direction>( cube_direction data )
{
    switch( data ) {
        // *INDENT-OFF*
        case cube_direction::north: return "north";
        case cube_direction::east: return "east";
        case cube_direction::south: return "south";
        case cube_direction::west: return "west";
        case cube_direction::above: return "above";
        case cube_direction::below: return "below";
        // *INDENT-ON*
        case cube_direction::last:
            break;
    }
    debugmsg( "Invalid cube_direction" );
    abort();
}

} // namespace io

namespace om_lines
{

struct type {
    uint32_t symbol;
    size_t mapgen;
    MULTITILE_TYPE subtile;
    int rotation;
    std::string suffix;
};

const std::array<std::string, 5> mapgen_suffixes = {{
        "_straight", "_curved", "_end", "_tee", "_four_way"
    }
};

const std::array < type, 1 + om_direction::bits > all = {{
        { UTF8_getch( LINE_XXXX_S ), 4, unconnected,  0, "_isolated"  }, // 0  ----
        { UTF8_getch( LINE_XOXO_S ), 2, end_piece,    2, "_end_south" }, // 1  ---n
        { UTF8_getch( LINE_OXOX_S ), 2, end_piece,    1, "_end_west"  }, // 2  --e-
        { UTF8_getch( LINE_XXOO_S ), 1, corner,       1, "_ne"        }, // 3  --en
        { UTF8_getch( LINE_XOXO_S ), 2, end_piece,    0, "_end_north" }, // 4  -s--
        { UTF8_getch( LINE_XOXO_S ), 0, edge,         0, "_ns"        }, // 5  -s-n
        { UTF8_getch( LINE_OXXO_S ), 1, corner,       0, "_es"        }, // 6  -se-
        { UTF8_getch( LINE_XXXO_S ), 3, t_connection, 1, "_nes"       }, // 7  -sen
        { UTF8_getch( LINE_OXOX_S ), 2, end_piece,    3, "_end_east"  }, // 8  w---
        { UTF8_getch( LINE_XOOX_S ), 1, corner,       2, "_wn"        }, // 9  w--n
        { UTF8_getch( LINE_OXOX_S ), 0, edge,         1, "_ew"        }, // 10 w-e-
        { UTF8_getch( LINE_XXOX_S ), 3, t_connection, 2, "_new"       }, // 11 w-en
        { UTF8_getch( LINE_OOXX_S ), 1, corner,       3, "_sw"        }, // 12 ws--
        { UTF8_getch( LINE_XOXX_S ), 3, t_connection, 3, "_nsw"       }, // 13 ws-n
        { UTF8_getch( LINE_OXXX_S ), 3, t_connection, 0, "_esw"       }, // 14 wse-
        { UTF8_getch( LINE_XXXX_S ), 4, center,       0, "_nesw"      }  // 15 wsen
    }
};

const size_t size = all.size();
const size_t invalid = 0;

constexpr size_t rotate( size_t line, om_direction::type dir )
{
    if( dir == om_direction::type::invalid ) {
        return line;
    }
    // Bitwise rotation to the left.
    return ( ( ( line << static_cast<size_t>( dir ) ) |
               ( line >> ( om_direction::size - static_cast<size_t>( dir ) ) ) ) & om_direction::bits );
}

constexpr size_t set_segment( size_t line, om_direction::type dir )
{
    if( dir == om_direction::type::invalid ) {
        return line;
    }
    return line | 1 << static_cast<size_t>( dir );
}

constexpr bool has_segment( size_t line, om_direction::type dir )
{
    if( dir == om_direction::type::invalid ) {
        return false;
    }
    return static_cast<bool>( line & 1 << static_cast<size_t>( dir ) );
}

constexpr bool is_straight( size_t line )
{
    return line == 1
           || line == 2
           || line == 4
           || line == 5
           || line == 8
           || line == 10;
}

static size_t from_dir( om_direction::type dir )
{
    switch( dir ) {
        case om_direction::type::north:
        case om_direction::type::south:
            return 5;  // ns;
        case om_direction::type::east:
        case om_direction::type::west:
            return 10; // ew
        case om_direction::type::invalid:
        case om_direction::type::last:
            debugmsg( "Can't retrieve a line from the invalid direction." );
    }

    return 0;
}

} // namespace om_lines

//const regional_settings default_region_settings;
t_regional_settings_map region_settings_map;

namespace
{

generic_factory<overmap_land_use_code> land_use_codes( "overmap land use codes" );
generic_factory<oter_type_t> terrain_types( "overmap terrain type" );
generic_factory<oter_t> terrains( "overmap terrain" );
generic_factory<overmap_special> specials( "overmap special" );

} // namespace

template<>
const overmap_land_use_code &overmap_land_use_code_id::obj() const
{
    return land_use_codes.obj( *this );
}

template<>
bool overmap_land_use_code_id::is_valid() const
{
    return land_use_codes.is_valid( *this );
}

template<>
const overmap_special &overmap_special_id::obj() const
{
    return specials.obj( *this );
}

template<>
bool overmap_special_id::is_valid() const
{
    return specials.is_valid( *this );
}

city::city( const point_om_omt &P, int const S )
    : pos( P )
    , size( S )
    , name( Name::get( nameIsTownName ) )
{
}

int city::get_distance_from( const tripoint_om_omt &p ) const
{
    return std::max( trig_dist( p, tripoint_om_omt{ pos, 0 } ) - size, 0 );
}

std::map<enum radio_type, std::string> radio_type_names =
{{ {radio_type::MESSAGE_BROADCAST, "broadcast"}, {radio_type::WEATHER_RADIO, "weather"} }};

radio_tower::radio_tower( const point_om_sm &p, int S, const std::string &M, radio_type T ) :
    pos( p ), strength( S ), type( T ), message( M )
{
    frequency = rng( 0, std::numeric_limits<int32_t>::max() );
}

/** @relates string_id */
template<>
bool string_id<oter_type_t>::is_valid() const
{
    return terrain_types.is_valid( *this );
}

/** @relates int_id */
template<>
const string_id<oter_type_t> &int_id<oter_type_t>::id() const
{
    return terrain_types.convert( *this );
}

/** @relates string_id */
template<>
int_id<oter_type_t> string_id<oter_type_t>::id() const
{
    return terrain_types.convert( *this, int_id<oter_type_t>( 0 ) );
}

/** @relates int_id */
template<>
int_id<oter_type_t>::int_id( const string_id<oter_type_t> &id ) : _id( id.id() ) {}

template<>
const oter_type_t &int_id<oter_type_t>::obj() const
{
    return terrain_types.obj( *this );
}

/** @relates string_id */
template<>
const oter_type_t &string_id<oter_type_t>::obj() const
{
    return terrain_types.obj( *this );
}

/** @relates string_id */
template<>
bool string_id<oter_t>::is_valid() const
{
    return terrains.is_valid( *this );
}

/** @relates string_id */
template<>
const oter_t &string_id<oter_t>::obj() const
{
    return terrains.obj( *this );
}

/** @relates string_id */
template<>
int_id<oter_t> string_id<oter_t>::id() const
{
    return terrains.convert( *this, ot_null );
}

/** @relates int_id */
template<>
int_id<oter_t>::int_id( const string_id<oter_t> &id ) : _id( id.id() ) {}

/** @relates int_id */
template<>
bool int_id<oter_t>::is_valid() const
{
    return terrains.is_valid( *this );
}

/** @relates int_id */
template<>
const oter_t &int_id<oter_t>::obj() const
{
    return terrains.obj( *this );
}

/** @relates int_id */
template<>
const string_id<oter_t> &int_id<oter_t>::id() const
{
    return terrains.convert( *this );
}

bool operator==( const int_id<oter_t> &lhs, const char *rhs )
{
    return lhs.id().str() == rhs;
}

bool operator!=( const int_id<oter_t> &lhs, const char *rhs )
{
    return !( lhs == rhs );
}

static void set_oter_ids()   // FIXME: constify
{
    ot_null         = oter_str_id::NULL_ID();
    // NOT required.
    ot_crater       = oter_id( "crater" );
    ot_field        = oter_id( "field" );
    ot_forest       = oter_id( "forest" );
    ot_forest_thick = oter_id( "forest_thick" );
    ot_forest_water = oter_id( "forest_water" );
    ot_river_center = oter_id( "river_center" );
}

std::string overmap_land_use_code::get_symbol() const
{
    return utf32_to_utf8( symbol );
}

void overmap_land_use_code::load( const JsonObject &jo, const std::string &src )
{
    const bool strict = is_strict_enabled( src );
    assign( jo, "land_use_code", land_use_code, strict );
    assign( jo, "name", name, strict );
    assign( jo, "detailed_definition", detailed_definition, strict );

    optional( jo, was_loaded, "sym", symbol, unicode_codepoint_from_symbol_reader, NULL_UNICODE );

    if( symbol == NULL_UNICODE ) {
        debugmsg( "sym is not defined properly for land_use_code %s (%s)", id.c_str(), name );
    }

    assign( jo, "color", color, strict );

}

void overmap_land_use_code::finalize()
{

}

void overmap_land_use_code::check() const
{

}

void overmap_land_use_codes::load( const JsonObject &jo, const std::string &src )
{
    land_use_codes.load( jo, src );
}

void overmap_land_use_codes::finalize()
{
    for( const auto &elem : land_use_codes.get_all() ) {
        const_cast<overmap_land_use_code &>( elem ).finalize(); // This cast is ugly, but safe.
    }
}

void overmap_land_use_codes::check_consistency()
{
    land_use_codes.check();
}

void overmap_land_use_codes::reset()
{
    land_use_codes.reset();
}

const std::vector<overmap_land_use_code> &overmap_land_use_codes::get_all()
{
    return land_use_codes.get_all();
}

void overmap_special_connection::deserialize( const JsonObject &jo )
{
    mandatory( jo, false, "point", p );
    if( jo.has_string( "terrain" ) ) {
        // Legacy migration.
        // TODO: remove after BN 0.F or 0.E (or whatever it's gonna be)
        if( json_report_strict ) {
            try {
                jo.throw_error( "Defining connection by terrain is deprecated, use explicit 'connection' instead.",
                                "terrain" );
            } catch( const JsonError &e ) {
                debugmsg( "%s", e.what() );
            }
        }

        if( !jo.has_member( "connection" ) ) {
            std::string t = jo.get_string( "terrain" );
            if( t == "sewer" ) {
                connection = overmap_connection_id( "sewer_tunnel" );
            } else if( t == "subway" ) {
                connection = overmap_connection_id( "subway_tunnel" );
            } else if( t == "forest_trail" ) {
                connection = overmap_connection_id( "forest_trail" );
            } else {
                connection = overmap_connection_id( "local_road" );
            }
        }
    } else {
        mandatory( jo, false, "connection", connection );
    }
    optional( jo, false, "existing", existing );
    optional( jo, false, "from", from );
}

void overmap_special_connection::finalize()
{
    // If the connection has a "from" hint specified, then figure out what the
    // resulting direction from the hinted location to the connection point is,
    // and use that as the intial direction to be passed off to the connection
    // building code.
    if( from ) {
        const direction calculated_direction = direction_from( *from, p );
        switch( calculated_direction ) {
            case direction::NORTH:
                initial_dir = cube_direction::north;
                break;
            case direction::EAST:
                initial_dir = cube_direction::east;
                break;
            case direction::SOUTH:
                initial_dir = cube_direction::south;
                break;
            case direction::WEST:
                initial_dir = cube_direction::west;
                break;
            default:
                // The only supported directions are north/east/south/west
                // as those are the four directions that overmap connections
                // can be made in. If the direction we figured out wasn't
                // one of those, just set this as invalid. We'll provide
                // a warning to the user/developer in overmap_special::check().
                initial_dir = cube_direction::last;
                break;
        }
    }
}

void overmap_spawns::deserialize( const JsonObject &jo )
{
    mandatory( jo, false, "group", group );
    mandatory( jo, false, "population", population );
}

void overmap_static_spawns::deserialize( const JsonObject &jo )
{
    overmap_spawns::deserialize( jo );
    mandatory( jo, false, "chance", chance );
}

void overmap_special_spawns::deserialize( const JsonObject &jo )
{
    overmap_spawns::deserialize( jo );
    jo.read( "radius", radius );
}

void overmap_specials::load( const JsonObject &jo, const std::string &src )
{
    specials.load( jo, src );
}

void city_buildings::load( const JsonObject &jo, const std::string &src )
{
    // Just an alias
    overmap_specials::load( jo, src );
}

void overmap_specials::finalize()
{
    for( const auto &elem : specials.get_all() ) {
        const_cast<overmap_special &>( elem ).finalize(); // This cast is ugly, but safe.
    }
}

void overmap_specials::finalize_mapgen_parameters()
{
    for( const auto &elem : specials.get_all() ) {
        // This cast is ugly, but safe.
        const_cast<overmap_special &>( elem ).finalize_mapgen_parameters();
    }
}

void overmap_specials::check_consistency()
{
    specials.check();
}

void overmap_specials::reset()
{
    specials.reset();
}

const std::vector<overmap_special> &overmap_specials::get_all()
{
    return specials.get_all();
}

overmap_special_batch overmap_specials::get_default_batch( const point_abs_om &origin )
{
    std::vector<const overmap_special *> res;

    res.reserve( specials.size() );
    for( const overmap_special &elem : specials.get_all() ) {
        if( elem.can_spawn() ) {
            res.push_back( &elem );
        }
    }

    return overmap_special_batch( origin, res );
}

bool is_river( const oter_id &ter )
{
    return ter->is_river();
}

bool is_river_or_lake( const oter_id &ter )
{
    return ter->is_river() || ter->is_lake() || ter->is_lake_shore();
}

bool is_ot_match( const std::string &name, const oter_id &oter,
                  const ot_match_type match_type )
{
    static const auto is_ot = []( const std::string & otype, const oter_id & oter ) {
        return otype == oter.id().str();
    };

    static const auto is_ot_type = []( const std::string & otype, const oter_id & oter ) {
        // Is a match if the base type is the same which will allow for handling rotations/linear features
        // but won't incorrectly match other locations that happen to contain the substring.
        return otype == oter->get_type_id().str();
    };

    static const auto is_ot_prefix = []( const std::string & otype, const oter_id & oter ) {
        const size_t oter_size = oter.id().str().size();
        const size_t compare_size = otype.size();
        if( compare_size > oter_size ) {
            return false;
        }

        const auto &oter_str = oter.id();
        if( oter_str.str().compare( 0, compare_size, otype ) != 0 ) {
            return false;
        }

        // check if it's a full match
        if( compare_size == oter_size ) {
            return true;
        }

        // only okay for partial if next char is an underscore
        return oter_str.str()[compare_size] == '_';
    };

    static const auto is_ot_subtype = []( const std::string & otype, const oter_id & oter ) {
        // Checks for any partial match.
        return strstr( oter.id().c_str(), otype.c_str() );
    };

    switch( match_type ) {
        case ot_match_type::exact:
            return is_ot( name, oter );
        case ot_match_type::type:
            return is_ot_type( name, oter );
        case ot_match_type::prefix:
            return is_ot_prefix( name, oter );
        case ot_match_type::contains:
            return is_ot_subtype( name, oter );
        default:
            return false;
    }
}

/*
 * load mapgen functions from an overmap_terrain json entry
 * suffix is for roads/subways/etc which have "_straight", "_curved", "_tee", "_four_way" function mappings
 */
static void load_overmap_terrain_mapgens( const JsonObject &jo, const std::string &id_base,
        const std::string &suffix = "" )
{
    const std::string fmapkey( id_base + suffix );
    const std::string jsonkey( "mapgen" + suffix );
    register_mapgen_function( fmapkey );
    if( jo.has_array( jsonkey ) ) {
        for( JsonObject jio : jo.get_array( jsonkey ) ) {
            // NOLINTNEXTLINE(cata-use-named-point-constants)
            load_and_add_mapgen_function( jio, fmapkey, point_zero, point( 1, 1 ) );
        }
    }
}

std::string oter_type_t::get_symbol() const
{
    return utf32_to_utf8( symbol );
}

void oter_type_t::load( const JsonObject &jo, const std::string &src )
{
    const bool strict = is_strict_enabled( src );

    optional( jo, was_loaded, "sym", symbol, unicode_codepoint_from_symbol_reader, NULL_UNICODE );

    assign( jo, "name", name, strict );
    assign( jo, "see_cost", see_cost, strict );
    assign( jo, "travel_cost", travel_cost, strict );
    assign( jo, "extras", extras, strict );
    assign( jo, "mondensity", mondensity, strict );
    assign( jo, "spawns", static_spawns, strict );
    assign( jo, "color", color, strict );
    assign( jo, "land_use_code", land_use_code, strict );

    if( jo.has_member( "looks_like" ) ) {
        std::vector<std::string> ll;
        if( jo.has_array( "looks_like" ) ) {
            jo.read( "looks_like", ll );
        } else if( jo.has_string( "looks_like" ) ) {
            const std::string one_look = jo.get_string( "looks_like" );
            ll.push_back( one_look );
        }
        looks_like = ll;
    } else if( jo.has_member( "copy-from" ) ) {
        looks_like.insert( looks_like.begin(), jo.get_string( "copy-from" ) );
    }

    const auto flag_reader = make_flag_reader( oter_flags_map, "overmap terrain flag" );
    optional( jo, was_loaded, "flags", flags, flag_reader );

    optional( jo, was_loaded, "connect_group", connect_group, string_reader{} );

    if( has_flag( oter_flags::line_drawing ) ) {
        if( has_flag( oter_flags::no_rotate ) ) {
            jo.throw_error( R"(Mutually exclusive flags: "NO_ROTATE" and "LINEAR".)" );
        }

        for( const auto &elem : om_lines::mapgen_suffixes ) {
            load_overmap_terrain_mapgens( jo, id.str(), elem );
        }

        if( symbol == NULL_UNICODE ) {
            // Default the sym for linear terrains to a specific value which
            // has special behaviour when using fallback ASCII tiles so as to
            // cause it to draw using the box drawing characters (see
            // load_ascii_set).
            symbol = LINE_XOXO_C;
        }
    } else {
        if( symbol == NULL_UNICODE && !jo.has_string( "abstract" ) ) {
            debugmsg( "sym is not defined for overmap_terrain %s (%s)", id.c_str(), name );
        }
        if( !jo.has_string( "sym" ) && jo.has_number( "sym" ) ) {
            debugmsg( "sym is defined as number instead of string for overmap_terrain %s (%s)", id.c_str(),
                      name );
        }
        load_overmap_terrain_mapgens( jo, id.str() );
    }
}

void oter_type_t::check() const
{

}

void oter_type_t::finalize()
{
    directional_peers.clear();  // In case of a second finalization.

    if( is_rotatable() ) {
        for( om_direction::type dir : om_direction::all ) {
            register_terrain( oter_t( *this, dir ), static_cast<size_t>( dir ), om_direction::size );
        }
    } else if( has_flag( oter_flags::line_drawing ) ) {
        for( size_t i = 0; i < om_lines::size; ++i ) {
            register_terrain( oter_t( *this, i ), i, om_lines::size );
        }
    } else {
        register_terrain( oter_t( *this ), 0, 1 );
    }
}

void oter_type_t::register_terrain( const oter_t &peer, size_t n, size_t max_n )
{
    assert( n < max_n );
    assert( peer.type_is( *this ) );

    directional_peers.resize( max_n );

    if( peer.id.is_valid() ) {
        directional_peers[n] = peer.id.id();
        debugmsg( "Can't register the new overmap terrain \"%s\". It already exists.", peer.id.c_str() );
    } else {
        directional_peers[n] = terrains.insert( peer ).id.id();
    }
}

oter_id oter_type_t::get_first() const
{
    assert( !directional_peers.empty() );
    return directional_peers.front();
}

oter_id oter_type_t::get_rotated( om_direction::type dir ) const
{
    if( dir == om_direction::type::invalid ) {
        debugmsg( "Invalid rotation was asked from overmap terrain \"%s\".", id.c_str() );
        return ot_null;
    } else if( dir == om_direction::type::none || !is_rotatable() ) {
        return directional_peers.front();
    }
    assert( directional_peers.size() == om_direction::size );
    return directional_peers[static_cast<size_t>( dir )];
}

oter_id oter_type_t::get_linear( size_t n ) const
{
    if( !has_flag( oter_flags::line_drawing ) ) {
        debugmsg( "Overmap terrain \"%s \" isn't drawn with lines.", id.c_str() );
        return ot_null;
    }
    if( n >= om_lines::size ) {
        debugmsg( "Invalid overmap line (%d) was asked from overmap terrain \"%s\".", n, id.c_str() );
        return ot_null;
    }
    assert( directional_peers.size() == om_lines::size );
    return directional_peers[n];
}

oter_t::oter_t() : oter_t( oter_type_t::null_type ) {}

oter_t::oter_t( const oter_type_t &type ) :
    type( &type ),
    id( type.id.str() ),
    symbol( type.symbol ),
    symbol_alt( type.land_use_code ? type.land_use_code->symbol : symbol ) {}

oter_t::oter_t( const oter_type_t &type, om_direction::type dir ) :
    type( &type ),
    id( type.id.str() + "_" + io::enum_to_string( dir ) ),
    dir( dir ),
    symbol( om_direction::rotate_symbol( type.symbol, dir ) ),
    symbol_alt( om_direction::rotate_symbol( type.land_use_code ? type.land_use_code->symbol :
                type.symbol, dir ) ),
    line( om_lines::from_dir( dir ) ) {}

oter_t::oter_t( const oter_type_t &type, size_t line ) :
    type( &type ),
    id( type.id.str() + om_lines::all[line].suffix ),
    symbol( om_lines::all[line].symbol ),
    symbol_alt( om_lines::all[line].symbol ),
    line( line ) {}

std::string oter_t::get_mapgen_id() const
{
    return type->has_flag( oter_flags::line_drawing )
           ? type->id.str() + om_lines::mapgen_suffixes[om_lines::all[line].mapgen]
           : type->id.str();
}

oter_id oter_t::get_rotated( om_direction::type dir ) const
{
    return type->has_flag( oter_flags::line_drawing )
           ? type->get_linear( om_lines::rotate( this->line, dir ) )
           : type->get_rotated( om_direction::add( this->dir, dir ) );
}

void oter_t::get_rotation_and_subtile( int &rotation, int &subtile ) const
{
    if( is_linear() ) {
        const om_lines::type &t = om_lines::all[line];
        rotation = t.rotation;
        subtile = t.subtile;
    } else if( is_rotatable() ) {
        rotation = om_direction::get_num_ccw_rotations( get_dir() );
        subtile = -1;
    } else {
        rotation = 0;
        subtile = -1;
    }
}

int oter_t::get_rotation() const
{
    if( is_linear() ) {
        const om_lines::type &t = om_lines::all[line];
        // It turns out the rotation used for linear things is the opposite of
        // the rotation used for other things.  Sigh.
        return ( 4 - t.rotation ) % 4;
    }
    if( is_rotatable() ) {
        return static_cast<int>( get_dir() );
    }
    return 0;
}

bool oter_t::type_is( const oter_type_id &type_id ) const
{
    return type->id.id() == type_id;
}

bool oter_t::type_is( const oter_type_t &type ) const
{
    return this->type == &type;
}

bool oter_t::has_connection( om_direction::type dir ) const
{
    // TODO: It's a DAMN UGLY hack. Remove it as soon as possible.
    static const oter_str_id road_manhole( "road_nesw_manhole" );
    if( id == road_manhole ) {
        return true;
    }
    return om_lines::has_segment( line, dir );
}

bool oter_t::is_hardcoded() const
{
    // TODO: This set only exists because so does the monstrous 'if-else' statement in @ref map::draw_map(). Get rid of both.
    static const std::set<std::string> hardcoded_mapgen = {
        "ants_lab",
        "ants_lab_stairs",
        "ice_lab",
        "ice_lab_stairs",
        "ice_lab_core",
        "ice_lab_finale",
        "central_lab",
        "central_lab_stairs",
        "central_lab_core",
        "central_lab_finale",
        "tower_lab",
        "tower_lab_stairs",
        "tower_lab_finale",
        "lab",
        "lab_core",
        "lab_stairs",
        "lab_finale",
        "looted_building",  // pseudo-terrain
        "mine",
        "mine_down",
        "mine_finale",
        "office_tower_1",
        "office_tower_1_entrance",
        "office_tower_b",
        "office_tower_b_entrance",
        "slimepit",
        "slimepit_down",
        "temple",
        "temple_finale",
        "temple_stairs"
    };

    return hardcoded_mapgen.contains( get_mapgen_id() );
}

void overmap_terrains::load( const JsonObject &jo, const std::string &src )
{
    terrain_types.load( jo, src );
}

void overmap_terrains::check_consistency()
{
    for( const auto &elem : terrain_types.get_all() ) {
        elem.check();
        if( elem.static_spawns.group && !elem.static_spawns.group.is_valid() ) {
            debugmsg( "Invalid monster group \"%s\" in spawns of \"%s\".", elem.static_spawns.group.c_str(),
                      elem.id.c_str() );
        }
    }

    for( const auto &elem : terrains.get_all() ) {
        const std::string mid = elem.get_mapgen_id();

        if( mid.empty() ) {
            continue;
        }

        const bool exists_hardcoded = elem.is_hardcoded();

        if( has_mapgen_for( mid ) ) {
            if( test_mode && exists_hardcoded ) {
                debugmsg( "Mapgen terrain \"%s\" exists in both JSON and a hardcoded function.  Consider removing the latter.",
                          mid.c_str() );
            }
        } else if( !exists_hardcoded ) {
            debugmsg( "No mapgen terrain exists for \"%s\".", mid.c_str() );
        }
    }
}

void overmap_terrains::finalize()
{
    terrain_types.finalize();

    for( const auto &elem : terrain_types.get_all() ) {
        const_cast<oter_type_t &>( elem ).finalize(); // This cast is ugly, but safe.
    }

    if( !region_settings_map.contains( "default" ) ) {
        debugmsg( "ERROR: can't find default overmap settings (region_map_settings 'default'), "
                  "cataclysm pending.  And not the fun kind." );
    }

    for( auto &elem : region_settings_map ) {
        elem.second.finalize();
    }

    set_oter_ids();
}

void overmap_terrains::reset()
{
    terrain_types.reset();
    terrains.reset();
}

const std::vector<oter_t> &overmap_terrains::get_all()
{
    return terrains.get_all();
}

static bool is_amongst_locations( const oter_id &oter,
                                  const cata::flat_set<overmap_location_id> &locations )
{
    return std::any_of( locations.begin(), locations.end(),
    [&oter]( const overmap_location_id & loc ) {
        return loc->test( oter );
    } );
}

bool overmap_special_locations::can_be_placed_on( const oter_id &oter ) const
{
    return is_amongst_locations( oter, locations );
}

void overmap_special_locations::deserialize( JsonIn &jsin )
{
    JsonArray ja = jsin.get_array();

    if( ja.size() != 2 ) {
        ja.throw_error( "expected array of size 2" );
    }

    ja.read( 0, p, true );
    ja.read( 1, locations, true );
}

void overmap_special_terrain::deserialize( JsonIn &jsin )
{
    JsonObject om = jsin.get_object();
    om.read( "point", p );
    om.read( "overmap", terrain );
    om.read( "locations", locations );
}

cube_direction operator+( const cube_direction l, const om_direction::type r )
{
    return l + static_cast<int>( r );
}

cube_direction operator+( const cube_direction d, int i )
{
    switch( d ) {
        case cube_direction::north:
        case cube_direction::east:
        case cube_direction::south:
        case cube_direction::west:
            return static_cast<cube_direction>( ( static_cast<int>( d ) + i ) % 4 );
        case cube_direction::above:
        case cube_direction::below:
            return d;
        case cube_direction::last:
            break;
    }
    debugmsg( "Invalid cube_direction" );
    abort();
}

cube_direction operator-( const cube_direction l, const om_direction::type r )
{
    return l - static_cast<int>( r );
}

cube_direction operator-( const cube_direction d, int i )
{
    switch( d ) {
        case cube_direction::north:
        case cube_direction::east:
        case cube_direction::south:
        case cube_direction::west:
            return static_cast<cube_direction>( ( static_cast<int>( d ) - i + 4 ) % 4 );
        case cube_direction::above:
        case cube_direction::below:
            return d;
        case cube_direction::last:
            break;
    }
    debugmsg( "Invalid cube_direction" );
    abort();
}

tripoint displace( cube_direction d )
{
    switch( d ) {
        case cube_direction::north:
            return tripoint_north;
        case cube_direction::east:
            return tripoint_east;
        case cube_direction::south:
            return tripoint_south;
        case cube_direction::west:
            return tripoint_west;
        case cube_direction::above:
            return tripoint_above;
        case cube_direction::below:
            return tripoint_below;
        case cube_direction::last:
            break;
    }
    debugmsg( "Invalid cube_direction" );
    abort();
}

struct placed_connection {
    overmap_connection_id connection;
    pos_dir<tripoint_om_omt> where;
};

struct special_placement_result {
    std::vector<tripoint_om_omt> omts_used;
    std::vector<placed_connection> cons_used;
    std::vector<std::pair<om_pos_dir, std::string>> joins_used;
};

struct overmap_special_data {
    virtual ~overmap_special_data() = default;
    virtual void finalize(
        const std::string &context,
        const cata::flat_set<overmap_location_id> &default_locations ) = 0;
    virtual void check( const std::string &context ) const = 0;
    virtual const oter_str_id &get_terrain_at( const tripoint &p ) const = 0;
    virtual std::vector<oter_str_id> all_terrains() const = 0;
    virtual std::vector<overmap_special_terrain> preview_terrains() const = 0;
    virtual std::vector<overmap_special_locations> required_locations() const = 0;
    virtual special_placement_result place(
        overmap &om, const tripoint_om_omt &origin, om_direction::type dir,
        const overmap_special &special ) const = 0;
};

struct fixed_overmap_special_data : overmap_special_data {
    std::vector<overmap_special_terrain> terrains;

    fixed_overmap_special_data() = default;
    explicit fixed_overmap_special_data( const overmap_special_terrain &ter )
        : terrains{ ter }
    {}

    void finalize(
        const std::string &/*context*/,
        const cata::flat_set<overmap_location_id> &default_locations ) override {
        // If the special has default locations, then add those to the locations
        // of each of the terrains IF the terrain has no locations already.
        for( auto &t : terrains ) {
            if( t.locations.empty() ) {
                t.locations = default_locations;
            }
        }
    }

    void check( const std::string &context ) const override {
        std::set<oter_str_id> invalid_terrains;
        std::set<tripoint> points;

        for( const overmap_special_terrain &elem : terrains ) {
            const oter_str_id &oter = elem.terrain;

            if( !oter.is_valid() ) {
                if( !invalid_terrains.contains( oter ) ) {
                    // Not a huge fan of the the direct id manipulation here, but I don't know
                    // how else to do this
                    oter_str_id invalid( oter.str() + "_north" );
                    if( invalid.is_valid() ) {
                        debugmsg( "In %s, terrain \"%s\" rotates, but is specified without a "
                                  "rotation.", context, oter.str() );
                    } else  {
                        debugmsg( "In %s, terrain \"%s\" is invalid.", context, oter.str() );
                    }
                    invalid_terrains.insert( oter );
                }
            }

            const auto &pos = elem.p;

            if( points.contains( pos ) ) {
                debugmsg( "In %s, point %s is duplicated.", context, pos.to_string() );
            } else {
                points.insert( pos );
            }

            if( elem.locations.empty() ) {
                debugmsg( "In %s, no location is defined for point %s or the "
                          "overall special.", context, pos.to_string() );
            }

            for( const auto &l : elem.locations ) {
                if( !l.is_valid() ) {
                    debugmsg( "In %s, point %s, location \"%s\" is invalid.",
                              context, pos.to_string(), l.c_str() );
                }
            }
        }
    }

    const oter_str_id &get_terrain_at( const tripoint &p ) const override {
        const auto iter = std::find_if( terrains.begin(), terrains.end(),
        [ &p ]( const overmap_special_terrain & elem ) {
            return elem.p == p;
        } );
        if( iter == terrains.end() ) {
            return oter_str_id::NULL_ID();
        }
        return iter->terrain;
    }

    std::vector<oter_str_id> all_terrains() const override {
        std::vector<oter_str_id> result;
        for( const overmap_special_terrain &ter : terrains ) {
            result.push_back( ter.terrain );
        }
        return result;
    }

    std::vector<overmap_special_terrain> preview_terrains() const override {
        std::vector<overmap_special_terrain> result;
        std::copy_if( terrains.begin(), terrains.end(), std::back_inserter( result ),
        []( const overmap_special_terrain & terrain ) {
            return terrain.p.z == 0;
        } );
        return result;
    }

    std::vector<overmap_special_locations> required_locations() const override {
        std::vector<overmap_special_locations> result;
        std::copy( terrains.begin(), terrains.end(), std::back_inserter( result ) );
        return result;
    }

    special_placement_result place(
        overmap &om, const tripoint_om_omt &origin, om_direction::type dir,
        const overmap_special &special ) const override {
        special_placement_result result;

        const bool blob = special.has_flag( "BLOB" );

        for( const auto &elem : terrains ) {
            const tripoint_om_omt location = origin + om_direction::rotate( elem.p, dir );
            result.omts_used.push_back( location );
            const oter_id tid = elem.terrain->get_rotated( dir );

            om.ter_set( location, tid );

            if( blob ) {
                for( int x = -2; x <= 2; x++ ) {
                    for( int y = -2; y <= 2; y++ ) {
                        const tripoint_om_omt nearby_pos = location + point( x, y );
                        if( !overmap::inbounds( nearby_pos ) ) {
                            continue;
                        }
                        if( one_in( 1 + std::abs( x ) + std::abs( y ) ) &&
                            elem.can_be_placed_on( om.ter( nearby_pos ) ) ) {
                            om.ter_set( nearby_pos, tid );
                        }
                    }
                }
            }
        }

        return result;
    }
};

struct mutable_overmap_join {
    std::string id;
    std::string opposite_id;
    cata::flat_set<overmap_location_id> into_locations;
    unsigned priority; // NOLINT(cata-serialize)
    const mutable_overmap_join *opposite = nullptr; // NOLINT(cata-serialize)

    void deserialize( JsonIn &jin ) {
        if( jin.test_string() ) {
            id = jin.get_string();
        } else {
            JsonObject jo = jin.get_object();
            jo.read( "id", id, true );
            jo.read( "into_locations", into_locations, true );
            jo.read( "opposite", opposite_id, true );
        }
    }
};

enum class join_type {
    mandatory,
    optional,
    available,
    reject,
    last
};

template<>
struct enum_traits<join_type> {
    static constexpr join_type last = join_type::last;
};

namespace io
{

template<>
std::string enum_to_string<join_type>( join_type data )
{
    switch( data ) {
        // *INDENT-OFF*
        case join_type::mandatory: return "mandatory";
        case join_type::optional: return "optional";
        case join_type::available: return "available";
        case join_type::reject: return "reject";
        // *INDENT-ON*
        case join_type::last:
            break;
    }
    debugmsg( "Invalid join_type" );
    abort();
}

} // namespace io

struct mutable_overmap_terrain_join {
    std::string join_id;
    const mutable_overmap_join *join = nullptr; // NOLINT(cata-serialize)
    cata::flat_set<std::string> alternative_join_ids;
    cata::flat_set<const mutable_overmap_join *> alternative_joins; // NOLINT(cata-serialize)
    join_type type = join_type::mandatory;

    void finalize( const std::string &context,
                   const std::unordered_map<std::string, mutable_overmap_join *> &joins ) {
        auto join_it = joins.find( join_id );
        if( join_it != joins.end() ) {
            join = join_it->second;
        } else {
            debugmsg( "invalid join id %s in %s", join_id, context );
        }
        alternative_joins.clear();
        for( const std::string &alt_join_id : alternative_join_ids ) {
            auto alt_join_it = joins.find( alt_join_id );
            if( alt_join_it != joins.end() ) {
                alternative_joins.insert( alt_join_it->second );
            } else {
                debugmsg( "invalid join id %s in %s", alt_join_id, context );
            }
        }
    }

    void deserialize( JsonIn &jin ) {
        if( jin.test_string() ) {
            jin.read( join_id, true );
        } else if( jin.test_object() ) {
            JsonObject jo = jin.get_object();
            jo.read( "id", join_id, true );
            jo.read( "type", type, true );
            jo.read( "alternatives", alternative_join_ids, true );
        } else {
            jin.error( "Expected string or object" );
        }
    }
};

using join_map = std::unordered_map<cube_direction, mutable_overmap_terrain_join>;

struct z_constraints {
    enum class constraint_type {
        any,
        range,
        top,
        bottom,
        offset
    };

    constraint_type type = constraint_type::any;
    int min = INT_MIN;
    int max = INT_MAX;

    bool check( const tripoint_om_omt &pos, const int z_min, const int z_max ) const {
        switch( type ) {
            case constraint_type::any:
                return true;
            case constraint_type::range:
                return pos.z() <= max && pos.z() >= min;
            case constraint_type::top:
                return pos.z() >= z_max;
            case constraint_type::bottom:
                return pos.z() <= z_min;
            case constraint_type::offset:
                return pos.z() == max + z_max || pos.z() == min + z_min;
        }
        return false;
    }

    void deserialize( JsonIn &jsin ) {
        if( jsin.test_int() ) {
            int v = jsin.get_int();
            min = v;
            max = v;
            type = constraint_type::range;
        } else if( jsin.test_array() ) {
            JsonArray ja = jsin.get_array();
            if( ja.size() != 2 ) {
                ja.throw_error( "Array should be in format [min, max]" );
            }
            min = ja.get_int( 0 );
            max = ja.get_int( 1 );
            type = constraint_type::range;
        } else if( jsin.test_string() ) {
            std::string type_string = jsin.get_string();
            if( type_string == "top" ) {
                type = constraint_type::top;
            } else if( type_string == "bottom" ) {
                type = constraint_type::bottom;
            } else {
                jsin.error( "String should be 'top' or 'bottom'" );
            }
        } else if( jsin.test_object() ) {
            JsonObject jo = jsin.get_object();
            jo.read( "top", max, true );
            jo.read( "bottom", min, true );
            type = constraint_type::offset;
            if( max == INT_MAX && min == INT_MIN ) {
                jo.throw_error( "Object should have at least one of 'top' and 'bottom' properties." );
            }
        } else {
            jsin.error( "Unrecognized z-constraints" );
        }
    }
};

struct mutable_special_connection {
    overmap_connection_id connection;

    void deserialize( const JsonObject &jo ) {
        jo.read( "connection", connection );
    }

    void check( const std::string &context ) const {
        if( !connection.is_valid() ) {
            debugmsg( "invalid connection id %s in %s", connection.str(), context );
        }
    }
};

struct mutable_overmap_terrain {
    oter_str_id terrain;
    cata::flat_set<overmap_location_id> locations;
    join_map joins;
    std::map<cube_direction, mutable_special_connection> connections;

    void finalize( const std::string &context,
                   const std::unordered_map<std::string, mutable_overmap_join *> &special_joins,
                   const cata::flat_set<overmap_location_id> &default_locations ) {
        if( locations.empty() ) {
            locations = default_locations;
        }
        for( join_map::value_type &p : joins ) {
            mutable_overmap_terrain_join &ter_join = p.second;
            ter_join.finalize( context, special_joins );
        }
    }

    void check( const std::string &context ) const {
        if( !terrain.is_valid() ) {
            debugmsg( "invalid overmap terrain id %s in %s", terrain.str(), context );
        }

        if( locations.empty() ) {
            debugmsg( "In %s, no locations are defined", context );
        }

        for( const overmap_location_id &loc : locations ) {
            if( !loc.is_valid() ) {
                debugmsg( "invalid overmap location id %s in %s", loc.str(), context );
            }
        }

        for( const std::pair<const cube_direction, mutable_special_connection> &p :
             connections ) {
            p.second.check( string_format( "connection %s in %s", io::enum_to_string( p.first ),
                                           context ) );
        }
    }

    void deserialize( JsonIn &jin ) {
        JsonObject jo = jin.get_object();
        jo.read( "overmap", terrain, true );
        jo.read( "locations", locations );
        for( int i = 0; i != static_cast<int>( cube_direction::last ); ++i ) {
            cube_direction dir = static_cast<cube_direction>( i );
            std::string dir_s = io::enum_to_string( dir );
            if( jo.has_member( dir_s ) ) {
                jo.read( dir_s, joins[dir], true );
            }
        }
        jo.read( "connections", connections );
    }
};

struct mutable_overmap_piece_candidate {
    const mutable_overmap_terrain *overmap; // NOLINT(cata-serialize)
    tripoint_om_omt pos;
    om_direction::type rot = om_direction::type::north;
};

struct mutable_overmap_placement_rule_piece {
    std::string overmap_id;
    const mutable_overmap_terrain *overmap; // NOLINT(cata-serialize)
    tripoint_rel_omt pos;
    om_direction::type rot = om_direction::type::north;

    void deserialize( const JsonObject &jo ) {
        jo.read( "overmap", overmap_id, true );
        jo.read( "pos", pos, true );
        jo.read( "rot", rot, true );
    }
};

struct mutable_overmap_placement_rule_remainder;

struct mutable_overmap_placement_rule {
    std::string name;
    std::string required_join;
    std::string scale;
    z_constraints z;
    std::optional<bool> rotate;
    std::optional<point_abs_om> om_pos;
    std::vector<mutable_overmap_placement_rule_piece> pieces;
    // NOLINTNEXTLINE(cata-serialize)
    std::vector<std::pair<rel_pos_dir, const mutable_overmap_terrain_join *>> outward_joins;
    int_distribution max = int_distribution( INT_MAX );
    int weight = INT_MAX;

    std::string description() const {
        if( !name.empty() ) {
            return name;
        }
        std::string first_om_id = pieces[0].overmap_id;
        if( pieces.size() == 1 ) {
            return first_om_id;
        } else {
            return "chunk using overmap " + first_om_id;
        }
    }

    void finalize( const std::string &context,
                   const std::unordered_map<std::string, mutable_overmap_terrain> &special_overmaps
                 ) {
        std::unordered_map<tripoint_rel_omt, const mutable_overmap_placement_rule_piece *>
        pieces_by_pos;
        for( mutable_overmap_placement_rule_piece &piece : pieces ) {
            bool inserted = pieces_by_pos.emplace( piece.pos, &piece ).second;
            if( !inserted ) {
                debugmsg( "phase of %s has chunk with duplicated position %s",
                          context, piece.pos.to_string() );
            }
            auto it = special_overmaps.find( piece.overmap_id );
            if( it == special_overmaps.end() ) {
                throw std::runtime_error(
                    string_format( "phase of %s specifies overmap %s which is not defined for that special",
                                   context, piece.overmap_id ) );
            } else {
                piece.overmap = &it->second;
            }
        }
        outward_joins.clear();
        for( const mutable_overmap_placement_rule_piece &piece : pieces ) {
            const mutable_overmap_terrain &ter = *piece.overmap;
            for( const join_map::value_type &p : ter.joins ) {
                const cube_direction dir = p.first;
                const mutable_overmap_terrain_join &ter_join = p.second;
                rel_pos_dir this_side{ piece.pos, dir + piece.rot };
                rel_pos_dir other_side = this_side.opposite();
                auto opposite_piece = pieces_by_pos.find( other_side.p );
                if( opposite_piece == pieces_by_pos.end() ) {
                    outward_joins.emplace_back( this_side, &ter_join );
                } else if( ter_join.type != join_type::mandatory ) {
                    // TODO: Validate rejects in chunks
                    continue;
                } else {
                    const std::string &opposite_join = ter_join.join->opposite_id;
                    const mutable_overmap_placement_rule_piece &other_piece =
                        *opposite_piece->second;
                    const mutable_overmap_terrain &other_om = *other_piece.overmap;

                    auto opposite_om_join =
                        other_om.joins.find( other_side.dir - other_piece.rot );
                    if( opposite_om_join == other_om.joins.end() ) {
                        debugmsg( "in phase of %s, %s has adjacent pieces %s at %s and %s at "
                                  "%s where the former has a join %s pointed towards the latter, "
                                  "but the latter has no join pointed towards the former",
                                  context, description(), piece.overmap_id, piece.pos.to_string(),
                                  other_piece.overmap_id, other_piece.pos.to_string(),
                                  ter_join.join_id );
                    } else if( opposite_om_join->second.join_id != opposite_join ) {
                        debugmsg( "in phase of %s, %s has adjacent pieces %s at %s and %s at "
                                  "%s where the former has a join %s pointed towards the latter, "
                                  "expecting a matching join %s wheras the latter has the join %s "
                                  "pointed towards the former",
                                  context, description(), piece.overmap_id, piece.pos.to_string(),
                                  other_piece.overmap_id, other_piece.pos.to_string(),
                                  ter_join.join_id, opposite_join,
                                  opposite_om_join->second.join_id );
                    }
                }
            }
        }
    }
    void check( const std::string &context,
                const std::unordered_map<std::string, mutable_overmap_join *> &joins,
                const std::unordered_map<std::string, int_distribution> &shared ) const {
        if( pieces.empty() ) {
            throw std::runtime_error( string_format( "phase of %s has chunk with zero pieces", context ) );
        }
        int min_max = max.minimum();
        if( min_max < 0 ) {
            debugmsg( "phase of %s specifies max which might be as low as %d; this should "
                      "be a positive number", context, min_max );
        }
        if( !required_join.empty() ) {
            auto join = joins.find( required_join );
            if( join == joins.end() ) {
                debugmsg( "invalid join id %s in phase of %s", required_join, context );
            }
        }
        if( !scale.empty() ) {
            auto dist = shared.find( scale );
            if( dist == shared.end() ) {
                debugmsg( "invalid shared multiplier %s in phase of %s", scale, context );
            }
        }
    }

    mutable_overmap_placement_rule_remainder realise( int scale ) const;

    void deserialize( const JsonObject &jo ) {
        jo.read( "name", name );
        if( jo.has_member( "overmap" ) ) {
            pieces.emplace_back();
            jo.read( "overmap", pieces.back().overmap_id, true );
        } else if( jo.has_member( "chunk" ) ) {
            jo.read( "chunk", pieces );
        } else {
            jo.throw_error( R"(placement rule must specify at least one of "overmap" or "chunk")" );
        }
        jo.read( "join", required_join );
        jo.read( "scale", scale );
        jo.read( "z", z );
        jo.read( "rotate", rotate );
        jo.read( "om_pos", om_pos );
        jo.read( "max", max );
        jo.read( "weight", weight );
    }
};

struct mutable_overmap_placement_rule_remainder {
    const mutable_overmap_placement_rule *parent;
    int max = INT_MAX;
    int weight = INT_MAX;

    std::string description() const {
        return parent->description();
    }

    int get_weight() const {
        return std::min( max, weight );
    }

    bool is_exhausted() const {
        return get_weight() == 0;
    }

    void decrement() {
        --max;
    }

    std::vector<tripoint_rel_omt> positions( om_direction::type rot ) const {
        std::vector<tripoint_rel_omt> result;
        for( const mutable_overmap_placement_rule_piece &piece : parent->pieces ) {
            result.push_back( rotate( piece.pos, rot ) );
        }
        return result;
    }
    std::vector<mutable_overmap_piece_candidate> pieces( const tripoint_om_omt &origin,
            om_direction::type rot ) const {
        std::vector<mutable_overmap_piece_candidate> result;
        for( const mutable_overmap_placement_rule_piece &piece : parent->pieces ) {
            tripoint_rel_omt rotated_offset = rotate( piece.pos, rot );
            result.push_back( { piece.overmap, origin + rotated_offset, add( rot, piece.rot ) } );
        }
        return result;
    }
    auto outward_joins( const tripoint_om_omt &origin, om_direction::type rot ) const
    -> std::vector<std::pair<om_pos_dir, const mutable_overmap_terrain_join *>> {
        std::vector<std::pair<om_pos_dir, const mutable_overmap_terrain_join *>> result;
        for( const std::pair<rel_pos_dir, const mutable_overmap_terrain_join *> &p :
             parent->outward_joins ) {
            tripoint_rel_omt rotated_offset = rotate( p.first.p, rot );
            om_pos_dir p_d{ origin + rotated_offset, p.first.dir + rot };
            result.emplace_back( p_d, p.second );
        }
        return result;
    }
};

mutable_overmap_placement_rule_remainder mutable_overmap_placement_rule::realise( int scale ) const
{
    return mutable_overmap_placement_rule_remainder{ this, max.sample( scale ), weight };
}

// When building a mutable overmap special we maintain a collection of
// unresolved joins.  We need to be able to index that collection in
// various ways, so it gets its own struct to maintain the relevant invariants.
class joins_tracker
{
    public:
        struct join {
            om_pos_dir where;
            join_type type;
            const mutable_overmap_join *join;
        };
        using iterator = std::list<join>::iterator;
        using const_iterator = std::list<join>::const_iterator;

        bool any_unresolved() const {
            return !unresolved.empty();
        }

        int mandatory_amount_at( const tripoint_om_omt &pos ) const {
            int ret = 0;
            for( iterator it : unresolved.all_at( pos ) ) {
                if( it->type == join_type::mandatory ) {
                    ret++;
                }
            }
            return ret;
        }

        std::vector<const join *> all_unresolved_at( const tripoint_om_omt &pos ) const {
            std::vector<const join *> result;
            for( iterator it : unresolved.all_at( pos ) ) {
                result.push_back( &*it );
            }
            return result;
        }

        bool is_finished() const {
            for( const auto &join : postponed ) {
                if( join.type == join_type::mandatory ) {
                    return false;
                }
            }
            return true;
        }

        bool any_postponed_at( const tripoint_om_omt &p ) const {
            return postponed.any_at( p );
        }

        void consistency_check() const {
            if( test_mode ) {
                // Enable this to check the class invariants, at the cost of more runtime
                // verify that there are no positions in common between the
                // resolved and postponed lists
                for( const join &j : postponed ) {
                    auto j_pos = j.where.p;
                    if( unresolved.any_at( j_pos ) ) {
                        std::vector<iterator> unr = unresolved.all_at( j_pos );
                        if( unr.empty() ) {
                            debugmsg( "inconsistency between all_at and any_at" );
                        } else {
                            const join &unr_j = *unr.front();
                            debugmsg( "postponed and unresolved should be disjoint but are not at "
                                      "%s where unresolved has %s: %s",
                                      j_pos.to_string(), unr_j.where.p.to_string(), unr_j.join->id );
                        }
                        abort();
                    }
                }
            }
        }

        enum class join_status {
            disallowed, // Conflicts with existing join, and at least one was mandatory
            matched_available, // Matches an existing non-mandatory join
            matched_non_available, // Matches an existing mandatory join
            mismatched_available, // Points at an incompatible join, but both are non-mandatory
            free, // Doesn't point at another join at all
        };

        join_status allows( const om_pos_dir &this_side,
                            const mutable_overmap_terrain_join &this_ter_join ) const {
            om_pos_dir other_side = this_side.opposite();

            auto is_allowed_opposite = [&]( const std::string & candidate ) {
                const mutable_overmap_join &this_join = *this_ter_join.join;

                if( this_join.opposite_id == candidate ) {
                    return true;
                }

                for( const mutable_overmap_join *alt_join : this_ter_join.alternative_joins ) {
                    if( alt_join->opposite_id == candidate ) {
                        return true;
                    }
                }

                return false;
            };

            if( const join *existing = resolved.find( other_side ) ) {
                join_type other_type = existing->type;
                if( is_allowed_opposite( existing->join->id ) ) {
                    if( other_type == join_type::mandatory ) {
                        return join_status::matched_non_available;
                    } else if( other_type == join_type::reject || this_ter_join.type == join_type::reject ) {
                        return join_status::disallowed;
                    } else {
                        return join_status::matched_available;
                    }
                } else {
                    if( other_type == join_type::mandatory || this_ter_join.type == join_type::mandatory ) {
                        return join_status::disallowed;
                    } else {
                        return join_status::mismatched_available;
                    }
                }
            } else {
                return join_status::free;
            }
        }

        void add_joins_for(
            const mutable_overmap_terrain &ter, const tripoint_om_omt &pos,
            om_direction::type rot, const std::vector<om_pos_dir> &suppressed_joins ) {
            consistency_check();

            std::unordered_set<om_pos_dir> avoid(
                suppressed_joins.begin(), suppressed_joins.end() );

            for( const std::pair<const cube_direction, mutable_overmap_terrain_join> &p :
                 ter.joins ) {
                cube_direction dir = p.first + rot;
                const mutable_overmap_terrain_join &this_side_join = p.second;

                om_pos_dir this_side{ pos, dir };
                om_pos_dir other_side = this_side.opposite();

                if( const join *other_side_join = resolved.find( other_side ) ) {
                    erase_unresolved( this_side );
                    if( !avoid.contains( this_side ) ) {
                        used.emplace_back( other_side, other_side_join->join->id );
                        // Because of the existence of alternative joins, we don't
                        // simply add this_side_join here, we add the opposite of
                        // the opposite that was actually present (this saves us
                        // from heaving to search through the alternates to find
                        // which one actually matched).
                        used.emplace_back( this_side, other_side_join->join->opposite_id );
                    }
                } else {
                    // If there were postponed joins pointing into this point,
                    // so we need to un-postpone them because it might now be
                    // possible to satisfy them.
                    restore_postponed_at( other_side.p );
                    if( this_side_join.type == join_type::mandatory ||
                        this_side_join.type == join_type::optional ) {
                        const mutable_overmap_join *opposite_join = this_side_join.join->opposite;
                        add_unresolved( other_side, this_side_join.type, opposite_join );
                    }
                }
                resolved.add( this_side, this_side_join.type, this_side_join.join );
            }
            consistency_check();
        }

        tripoint_om_omt pick_top_priority() const {
            assert( any_unresolved() );
            auto priority_it =
                std::find_if( unresolved_priority_index.begin(), unresolved_priority_index.end(),
            []( const cata::flat_set<iterator, compare_iterators> &its ) {
                return !its.empty();
            } );
            assert( priority_it != unresolved_priority_index.end() );
            auto it = random_entry( *priority_it );
            const tripoint_om_omt &pos = it->where.p;
            assert( !postponed.any_at( pos ) );
            return pos;
        }
        void postpone( const tripoint_om_omt &pos ) {
            consistency_check();
            for( iterator it : unresolved.all_at( pos ) ) {
                postponed.add( *it );
                [[maybe_unused]] const bool erased = erase_unresolved( it->where );
                assert( erased );
            }
            consistency_check();
        }
        void restore_postponed_at( const tripoint_om_omt &pos ) {
            for( iterator it : postponed.all_at( pos ) ) {
                add_unresolved( it->where, it->type, it->join );
                postponed.erase( it );
            }
            consistency_check();
        }
        void restore_postponed() {
            consistency_check();
            for( const join &j : postponed ) {
                add_unresolved( j.where, j.type, j.join );
            }
            postponed.clear();
        }

        const std::vector<std::pair<om_pos_dir, std::string>> &all_used() const {
            return used;
        }
    private:
        struct indexed_joins {
            std::list<join> joins;
            std::unordered_map<om_pos_dir, iterator> position_index;

            iterator begin() {
                return joins.begin();
            }

            iterator end() {
                return joins.end();
            }

            const_iterator begin() const {
                return joins.begin();
            }

            const_iterator end() const {
                return joins.end();
            }

            bool empty() const {
                return joins.empty();
            }

            bool count( const om_pos_dir &p ) const {
                return position_index.contains( p );
            }

            const join *find( const om_pos_dir &p ) const {
                auto it = position_index.find( p );
                if( it == position_index.end() ) {
                    return nullptr;
                }
                return &*it->second;
            }

            bool any_at( const tripoint_om_omt &pos ) const {
                for( cube_direction dir : all_enum_values<cube_direction>() ) {
                    if( count( om_pos_dir{ pos, dir } ) ) {
                        return true;
                    }
                }
                return false;
            }

            std::vector<iterator> all_at( const tripoint_om_omt &pos ) const {
                std::vector<iterator> result;
                for( cube_direction dir : all_enum_values<cube_direction>() ) {
                    om_pos_dir key{ pos, dir };
                    auto pos_it = position_index.find( key );
                    if( pos_it != position_index.end() ) {
                        result.push_back( pos_it->second );
                    }
                }
                return result;
            }

            iterator add( const om_pos_dir &p, const join_type &t, const mutable_overmap_join *j ) {
                return add( { p, t, j } );
            }

            iterator add( const join &j ) {
                joins.push_front( j );
                auto it = joins.begin();
                [[maybe_unused]] const bool inserted = position_index.emplace( j.where, it ).second;
                assert( inserted );
                return it;
            }

            void erase( const iterator it ) {
                [[maybe_unused]] const size_t erased = position_index.erase( it->where );
                assert( erased );
                joins.erase( it );
            }

            void clear() {
                joins.clear();
                position_index.clear();
            }
        };

        void add_unresolved( const om_pos_dir &p, const join_type &t, const mutable_overmap_join *j ) {
            iterator it = unresolved.add( p, t, j );
            unsigned priority = it->join->priority;
            if( unresolved_priority_index.size() <= priority ) {
                unresolved_priority_index.resize( priority + 1 );
            }
            [[maybe_unused]] const bool inserted = unresolved_priority_index[priority].insert( it ).second;
            assert( inserted );
        }

        bool erase_unresolved( const om_pos_dir &p ) {
            auto pos_it = unresolved.position_index.find( p );
            if( pos_it == unresolved.position_index.end() ) {
                return false;
            }
            iterator it = pos_it->second;
            unsigned priority = it->join->priority;
            assert( priority < unresolved_priority_index.size() );
            [[maybe_unused]] const size_t erased = unresolved_priority_index[priority].erase( it );
            assert( erased );
            unresolved.erase( it );
            return true;
        }

        struct compare_iterators {
            bool operator()( iterator l, iterator r ) {
                return l->where < r->where;
            }
        };

        indexed_joins unresolved;
        std::vector<cata::flat_set<iterator, compare_iterators>> unresolved_priority_index;

        indexed_joins resolved;
        indexed_joins postponed;

        std::vector<std::pair<om_pos_dir, std::string>> used;
};

struct mutable_overmap_phase_remainder {
    std::vector<mutable_overmap_placement_rule_remainder> rules;

    struct satisfy_result {
        tripoint_om_omt origin;
        om_direction::type dir;
        mutable_overmap_placement_rule_remainder *rule;
        std::vector<om_pos_dir> suppressed_joins;
        // For debugging purposes it's really handy to have a record of exactly
        // what happened during placement of a mutable special when it fails,
        // so to aid that we provide a human-readable description here which is
        // only used in the event of a placement error.
        std::string description;
    };

    bool all_rules_exhausted() const {
        return std::all_of( rules.begin(), rules.end(),
        []( const mutable_overmap_placement_rule_remainder & rule ) {
            return rule.is_exhausted();
        } );
    }

    struct can_place_result {
        int num_context_mandatory_joins_matched;
        int num_my_non_available_matched;
        std::vector<om_pos_dir> supressed_joins;

        std::pair<int, int> as_pair() const {
            return { num_context_mandatory_joins_matched, num_my_non_available_matched };
        }

        friend bool operator==( const can_place_result &l, const can_place_result &r ) {
            return l.as_pair() == r.as_pair();
        }

        friend bool operator<( const can_place_result &l, const can_place_result &r ) {
            return l.as_pair() < r.as_pair();
        }
    };

    std::optional<can_place_result> can_place(
        const overmap &om, const mutable_overmap_placement_rule_remainder &rule,
        const tripoint_om_omt &origin, om_direction::type dir,
        const joins_tracker &unresolved
    ) const {
        std::vector<mutable_overmap_piece_candidate> pieces = rule.pieces( origin, dir );
        int context_mandatory_joins_shortfall = 0;

        bool have_required_join = rule.parent->required_join.empty();
        for( const mutable_overmap_piece_candidate &piece : pieces ) {
            if( !overmap::inbounds( piece.pos ) ) {
                return std::nullopt;
            }
            if( !is_amongst_locations( om.ter( piece.pos ), piece.overmap->locations ) ) {
                return std::nullopt;
            }
            if( unresolved.any_postponed_at( piece.pos ) ) {
                return std::nullopt;
            }
            if( !have_required_join ) {
                for( const auto &join : piece.overmap->joins ) {
                    if( join.second.join_id == rule.parent->required_join ) {
                        have_required_join = true;
                        break;
                    }
                }
            }
            context_mandatory_joins_shortfall -= unresolved.mandatory_amount_at( piece.pos );
        }
        if( !have_required_join ) {
            return std::nullopt;
        }

        int num_my_non_available_matched = 0;

        std::vector<std::pair<om_pos_dir, const mutable_overmap_terrain_join *>> remaining_joins =
                    rule.outward_joins( origin, dir );
        std::vector<om_pos_dir> suppressed_joins;

        for( const std::pair<om_pos_dir, const mutable_overmap_terrain_join *> &p :
             remaining_joins ) {
            const om_pos_dir &pos_d = p.first;
            const mutable_overmap_terrain_join &ter_join = *p.second;
            const mutable_overmap_join &join = *ter_join.join;
            switch( unresolved.allows( pos_d, ter_join ) ) {
                case joins_tracker::join_status::disallowed:
                    return std::nullopt;
                case joins_tracker::join_status::matched_non_available:
                    ++context_mandatory_joins_shortfall;
                // fallthrough
                case joins_tracker::join_status::matched_available:
                    if( ter_join.type == join_type::mandatory ) {
                        ++num_my_non_available_matched;
                    }
                    continue;
                case joins_tracker::join_status::mismatched_available:
                    suppressed_joins.push_back( pos_d );
                // fallthrough
                case joins_tracker::join_status::free:
                    if( join.id == rule.parent->required_join ) {
                        return std::nullopt;
                    }
                    break;
            }
            if( ter_join.type != join_type::mandatory ) {
                continue;
            }
            // Verify that the remaining joins lead to
            // suitable locations
            tripoint_om_omt neighbour = pos_d.p + displace( pos_d.dir );
            if( !overmap::inbounds( neighbour ) ) {
                return std::nullopt;
            }
            const oter_id &neighbour_terrain = om.ter( neighbour );
            if( !is_amongst_locations( neighbour_terrain, join.into_locations ) ) {
                return std::nullopt;
            }
        }
        return can_place_result{ context_mandatory_joins_shortfall,
                                 num_my_non_available_matched, suppressed_joins };
    }

    satisfy_result satisfy( const overmap &om, const tripoint_om_omt &pos,
                            const joins_tracker &unresolved, const bool rotatable,
                            const int z_min, const int z_max ) {
        weighted_int_list<satisfy_result> options;

        for( mutable_overmap_placement_rule_remainder &rule : rules ) {
            std::vector<satisfy_result> pos_dir_options;
            can_place_result best_result{ 0, 0, {} };

            if( !rule.parent->z.check( pos, z_min, z_max ) ) {
                continue;
            }

            for( om_direction::type dir : om_direction::all ) {
                for( const tripoint_rel_omt &piece_pos : rule.positions( dir ) ) {
                    tripoint_om_omt origin = pos - piece_pos;

                    if( std::optional<can_place_result> result = can_place(
                                om, rule, origin, dir, unresolved ) ) {
                        if( best_result < *result ) {
                            pos_dir_options.clear();
                            best_result = *result;
                        }
                        if( *result == best_result ) {
                            pos_dir_options.push_back(
                                satisfy_result{ origin, dir, &rule, result.value().supressed_joins,
                                                {} } );
                        }
                    }
                }

                if( rule.parent->rotate ? !( *rule.parent->rotate ) : !rotatable ) {
                    break;
                }
            }

            if( auto chosen_result = random_entry_opt( pos_dir_options ) ) {
                options.add( *chosen_result, rule.get_weight() );
            }
        }
        std::string joins_s = enumerate_as_string( unresolved.all_unresolved_at( pos ),
        []( const joins_tracker::join * j ) {
            return string_format( "%s: %s", io::enum_to_string( j->where.dir ), j->join->id );
        } );

        if( satisfy_result *picked = options.pick() ) {
            om_direction::type dir = picked->dir;
            const mutable_overmap_placement_rule_remainder &rule = *picked->rule;
            picked->description =
                string_format(
                    "At %s chose '%s' rot %d with neighbours N:%s E:%s S:%s W:%s and constraints "
                    "%s",
                    pos.to_string(), rule.description(), static_cast<int>( dir ),
                    om.ter( pos + point_north ).id().str(), om.ter( pos + point_east ).id().str(),
                    om.ter( pos + point_south ).id().str(), om.ter( pos + point_west ).id().str(),
                    joins_s );
            picked->rule->decrement();
            return *picked;
        } else {
            std::string rules_s = enumerate_as_string( rules,
            []( const mutable_overmap_placement_rule_remainder & rule ) {
                if( rule.is_exhausted() ) {
                    return string_format( "(%s)", rule.description() );
                } else {
                    return rule.description();
                }
            } );
            std::string message =
                string_format(
                    "At %s FAILED to match on terrain %s with neighbours N:%s E:%s S:%s W:%s and "
                    "constraints %s from amongst rules %s",
                    pos.to_string(), om.ter( pos ).id().str(),
                    om.ter( pos + point_north ).id().str(), om.ter( pos + point_east ).id().str(),
                    om.ter( pos + point_south ).id().str(), om.ter( pos + point_west ).id().str(),
                    joins_s, rules_s );
            return { {}, om_direction::type::invalid, nullptr, {}, std::move( message ) };
        }
    }
};

struct mutable_overmap_phase {
    std::vector<mutable_overmap_placement_rule> rules;

    mutable_overmap_phase_remainder realise( overmap &om,
            std::unordered_map<std::string, int> scales ) const {
        std::vector<mutable_overmap_placement_rule_remainder> realised_rules;
        for( const mutable_overmap_placement_rule &rule : rules ) {
            if( rule.om_pos && *rule.om_pos != om.pos() ) {
                continue;
            }
            int scale = !rule.scale.empty() ? scales[rule.scale] : 1;
            realised_rules.push_back( rule.realise( scale ) );
        }
        return { realised_rules };
    }

    void deserialize( JsonIn &jin ) {
        jin.read( rules, true );
    }
};

template<typename Tripoint>
pos_dir<Tripoint> pos_dir<Tripoint>::opposite() const
{
    switch( dir ) {
        case cube_direction::north:
            return { p + tripoint_north, cube_direction::south };
        case cube_direction::east:
            return { p + tripoint_east, cube_direction::west };
        case cube_direction::south:
            return { p + tripoint_south, cube_direction::north };
        case cube_direction::west:
            return { p + tripoint_west, cube_direction::east };
        case cube_direction::above:
            return { p + tripoint_above, cube_direction::below };
        case cube_direction::below:
            return { p + tripoint_below, cube_direction::above };
        case cube_direction::last:
            break;
    }
    debugmsg( "Invalid cube_direction" );
    abort();
}

template<typename Tripoint>
void pos_dir<Tripoint>::serialize( JsonOut &jsout ) const
{
    jsout.start_array();
    jsout.write( p );
    jsout.write( dir );
    jsout.end_array();
}

template<typename Tripoint>
void pos_dir<Tripoint>::deserialize( JsonIn &jsin )
{
    JsonArray ja = jsin.get_array();
    if( ja.size() != 2 ) {
        ja.throw_error( "Expected array of size 2" );
    }
    ja.read( 0, p );
    ja.read( 1, dir );
}

template<typename Tripoint>
bool pos_dir<Tripoint>::operator==( const pos_dir<Tripoint> &r ) const
{
    return p == r.p && dir == r.dir;
}

template<typename Tripoint>
bool pos_dir<Tripoint>::operator<( const pos_dir<Tripoint> &r ) const
{
    return std::tie( p, dir ) < std::tie( r.p, r.dir );
}

template struct pos_dir<tripoint_om_omt>;
template struct pos_dir<tripoint_rel_omt>;

struct mutable_overmap_special_data : overmap_special_data {
    std::vector<overmap_special_locations> check_for_locations;
    std::vector<mutable_overmap_join> joins_vec;
    std::unordered_map<std::string, int_distribution> shared_dist;
    std::unordered_map<std::string, mutable_overmap_join *> joins;
    std::unordered_map<std::string, mutable_overmap_terrain> overmaps;
    std::string root;
    std::vector<mutable_overmap_phase> phases;

    void finalize( const std::string &context,
                   const cata::flat_set<overmap_location_id> &default_locations ) override {
        if( check_for_locations.empty() ) {
            check_for_locations.push_back( root_as_overmap_special_terrain() );
        }
        joins.clear();
        for( size_t i = 0; i != joins_vec.size(); ++i ) {
            mutable_overmap_join &join = joins_vec[i];
            if( join.into_locations.empty() ) {
                join.into_locations = default_locations;
            }
            join.priority = i;
            joins.emplace( join.id, &join );
        }
        for( mutable_overmap_join &join : joins_vec ) {
            if( join.opposite_id.empty() ) {
                join.opposite_id = join.id;
                join.opposite = &join;
                continue;
            }
            auto opposite_it = joins.find( join.opposite_id );
            if( opposite_it == joins.end() ) {
                // Error reported later in check()
                continue;
            }
            join.opposite = opposite_it->second;
        }
        for( std::pair<const std::string, mutable_overmap_terrain> &p : overmaps ) {
            mutable_overmap_terrain &ter = p.second;
            ter.finalize( string_format( "overmap %s in %s", p.first, context ), joins,
                          default_locations );
        }
        for( mutable_overmap_phase &phase : phases ) {
            for( mutable_overmap_placement_rule &rule : phase.rules ) {
                rule.finalize( context, overmaps );
            }
        }
    }

    void check( const std::string &context ) const override {
        if( joins_vec.size() != joins.size() ) {
            debugmsg( "duplicate join id in %s", context );
        }
        for( const mutable_overmap_join &join : joins_vec ) {
            if( join.opposite ) {
                if( join.opposite->opposite_id != join.id ) {
                    debugmsg( "in %1$s: join id %2$s specifies its opposite to be %3$s, but "
                              "the opposite of %3$s is %4$s, when it should match the "
                              "original id %2$s",
                              context, join.id, join.opposite_id, join.opposite->opposite_id );
                }
            } else {
                debugmsg( "in %s: join id '%s' specified as opposite of '%s' not valid",
                          context, join.opposite_id, join.id );
            }
        }
        for( const std::pair<const std::string, mutable_overmap_terrain> &p : overmaps ) {
            const mutable_overmap_terrain &ter = p.second;
            ter.check( string_format( "overmap %s in %s", p.first, context ) );
        }
        if( !overmaps.contains( root ) ) {
            debugmsg( "root %s is not amongst the defined overmaps for %s", root, context );
        }
        for( const mutable_overmap_phase &phase : phases ) {
            for( const mutable_overmap_placement_rule &rule : phase.rules ) {
                rule.check( context, joins, shared_dist );
            }
        }
    }

    const oter_str_id &get_terrain_at( const tripoint &p ) const override {
        auto it = overmaps.find( root );
        if( p != tripoint_zero || it == overmaps.end() ) {
            return oter_str_id::NULL_ID();
        }
        return it->second.terrain;
    }

    overmap_special_terrain root_as_overmap_special_terrain() const {
        auto it = overmaps.find( root );
        if( it == overmaps.end() ) {
            debugmsg( "root '%s' is not an overmap in this special", root );
            return {};
        }
        const mutable_overmap_terrain &root_om = it->second;
        return { tripoint_zero, root_om.terrain, root_om.locations };
    }

    std::vector<oter_str_id> all_terrains() const override {
        std::vector<oter_str_id> result;
        for( const auto &ter : overmaps ) {
            result.push_back( ter.second.terrain );
        }
        return result;
    }

    std::vector<overmap_special_terrain> preview_terrains() const override {
        return std::vector<overmap_special_terrain> { root_as_overmap_special_terrain() };
    }

    std::vector<overmap_special_locations> required_locations() const override {
        return check_for_locations;
    }

    // Returns a list of the points placed and a list of the joins used
    special_placement_result place(
        overmap &om, const tripoint_om_omt &origin, om_direction::type dir,
        const overmap_special &special ) const override {
        std::vector<tripoint_om_omt> result;
        std::vector<placed_connection> connections_placed;

        const bool rotatable = special.is_rotatable();

        auto it = overmaps.find( root );
        if( it == overmaps.end() ) {
            debugmsg( "Invalid root %s", root );
            return { result, {}, {} };
        }

        joins_tracker unresolved;

        // This is for debugging only, it tracks a human-readable description
        // of what happened to be put in the debugmsg in the event of failure.
        std::vector<std::string> descriptions;

        std::unordered_map<std::string, int> scales;
        for( const auto &dist : shared_dist ) {
            scales[dist.first] = dist.second.sample();
        }

        int z_min = INT_MAX;
        int z_max = INT_MIN;

        // Helper function to add a particular mutable_overmap_terrain at a
        // particular place.
        auto add_ter = [&](
                           const mutable_overmap_terrain & ter, const tripoint_om_omt & pos,
        om_direction::type rot, const std::vector<om_pos_dir> &suppressed_joins ) {
            const oter_id tid = ter.terrain->get_rotated( rot );
            om.ter_set( pos, tid );
            unresolved.add_joins_for( ter, pos, rot, suppressed_joins );
            result.push_back( pos );
            z_min = std::min( z_min, pos.z() );
            z_max = std::max( z_max, pos.z() );

            // Accumulate connections to be dealt with later
            for( const std::pair<const cube_direction, mutable_special_connection> &p :
                 ter.connections ) {
                cube_direction base_dir = p.first;
                const mutable_special_connection &conn = p.second;
                cube_direction dir = base_dir + rot;
                tripoint_om_omt conn_pos = pos + displace( dir );
                if( overmap::inbounds( conn_pos ) ) {
                    connections_placed.push_back( { conn.connection, { conn_pos, dir } } );
                }
            }
        };

        const mutable_overmap_terrain &root_omt = it->second;
        add_ter( root_omt, origin, dir, {} );

        auto current_phase = phases.begin();
        mutable_overmap_phase_remainder phase_remaining = current_phase->realise( om, scales );

        while( unresolved.any_unresolved() ) {
            tripoint_om_omt next_pos = unresolved.pick_top_priority();
            mutable_overmap_phase_remainder::satisfy_result satisfy_result =
                phase_remaining.satisfy( om, next_pos, unresolved, rotatable, z_min, z_max );
            descriptions.push_back( std::move( satisfy_result.description ) );
            const mutable_overmap_placement_rule_remainder *rule = satisfy_result.rule;
            if( rule ) {
                const tripoint_om_omt &origin = satisfy_result.origin;
                om_direction::type rot = satisfy_result.dir;
                std::vector<mutable_overmap_piece_candidate> pieces = rule->pieces( origin, rot );
                for( const mutable_overmap_piece_candidate &piece : pieces ) {
                    const mutable_overmap_terrain &ter = *piece.overmap;
                    add_ter( ter, piece.pos, piece.rot, satisfy_result.suppressed_joins );
                }
            } else {
                unresolved.postpone( next_pos );
            }
            if( !unresolved.any_unresolved() || phase_remaining.all_rules_exhausted() ) {
                ++current_phase;
                if( current_phase == phases.end() ) {
                    break;
                }
                descriptions.push_back(
                    string_format( "## Entering phase %td", current_phase - phases.begin() ) );
                phase_remaining = current_phase->realise( om, scales );
                unresolved.restore_postponed();
            }
        }

        if( !unresolved.is_finished() ) {
            // This is an error in the JSON; extract some useful info to help
            // the user debug it
            unresolved.restore_postponed();
            tripoint_om_omt p = unresolved.pick_top_priority();

            const oter_id &current_terrain = om.ter( p );
            std::string joins = enumerate_as_string( unresolved.all_unresolved_at( p ),
            []( const joins_tracker::join * dir_join ) {
                return string_format( "%s: %s", io::enum_to_string( dir_join->where.dir ),
                                      dir_join->join->id );
            } );

            debugmsg( "Spawn of mutable special %s had unresolved joins.  Existing terrain "
                      "at %s was %s; joins were %s\nComplete record of placement follows:\n%s",
                      special.id.str(), p.to_string(), current_terrain.id().str(), joins,
                      join( descriptions, "\n" ) );

            om.add_note(
                p, string_format(
                    "U:R;DEBUG: unresolved joins %s at %s placing %s",
                    joins, p.to_string(), special.id.str() ) );
        }

        return { result, connections_placed, unresolved.all_used() };
    }
};

overmap_special::overmap_special( const overmap_special_id &i, const overmap_special_terrain &ter )
    : id( i )
    , subtype_( overmap_special_subtype::fixed )
    , data_{ make_shared_fast<fixed_overmap_special_data>( ter ) }
{}

bool overmap_special::can_spawn() const
{
    if( get_constraints().occurrences.empty() ) {
        return false;
    }

    const int city_size = get_option<int>( "CITY_SIZE" );
    return city_size != 0 || get_constraints().city_size.min <= city_size;
}

bool overmap_special::requires_city() const
{
    return constraints_.city_size.min > 0 ||
           constraints_.city_distance.max < std::max( OMAPX, OMAPY );
}

bool overmap_special::can_belong_to_city( const tripoint_om_omt &p, const city &cit ) const
{
    return constraints_.city_distance.contains( cit.get_distance_from( p ) - ( cit.size ) );
}

int overmap_special::longest_side() const
{
    // Figure out the longest side of the special for purposes of determining our sector size
    // when attempting placements.
    std::vector<overmap_special_locations> req_locations = required_locations();
    auto min_max_x = std::minmax_element( req_locations.begin(), req_locations.end(),
    []( const overmap_special_locations & lhs, const overmap_special_locations & rhs ) {
        return lhs.p.x < rhs.p.x;
    } );

    auto min_max_y = std::minmax_element( req_locations.begin(), req_locations.end(),
    []( const overmap_special_locations & lhs, const overmap_special_locations & rhs ) {
        return lhs.p.y < rhs.p.y;
    } );

    const int width = min_max_x.second->p.x - min_max_x.first->p.x;
    const int height = min_max_y.second->p.y - min_max_y.first->p.y;
    return std::max( width, height ) + 1;
}

const oter_str_id &overmap_special::get_terrain_at( const tripoint &p ) const
{
    return data_->get_terrain_at( p );
}

std::vector<oter_str_id> overmap_special::all_terrains() const
{
    std::vector<oter_str_id> result = data_->all_terrains();

    for( const auto &nested : get_nested_specials() ) {
        auto inner = nested.second->all_terrains();
        result.insert( result.end(), std::make_move_iterator( inner.begin() ),
                       std::make_move_iterator( inner.end() ) );
    }
    return result;
}

std::vector<overmap_special_terrain> overmap_special::preview_terrains() const
{
    std::vector<overmap_special_terrain> result = data_->preview_terrains();

    for( const auto &nested : get_nested_specials() ) {
        for( const auto &ter : nested.second->preview_terrains() ) {
            overmap_special_terrain rel_ter = ter;
            rel_ter.p += nested.first.raw();
            if( rel_ter.p.z == 0 ) {
                result.push_back( rel_ter );
            }
        }
    }
    return result;
}

std::vector<overmap_special_locations> overmap_special::required_locations() const
{
    // TODO: It's called a lot during mapgen, and should probably be cached
    // instead of making thousands of copies
    std::vector<overmap_special_locations> result = data_->required_locations();

    for( const auto &nested : get_nested_specials() ) {
        for( const auto &loc : nested.second->required_locations() ) {
            overmap_special_locations rel_loc = loc;
            rel_loc.p += nested.first.raw();
            result.push_back( rel_loc );
        }
    }
    return result;
}


special_placement_result overmap_special::place(
    overmap &om, const tripoint_om_omt &origin, om_direction::type dir ) const
{
    return data_->place( om, origin, dir, *this );
}

mapgen_arguments overmap_special::get_args( const mapgendata &md ) const
{
    return mapgen_params_.get_args( md, mapgen_parameter_scope::overmap_special );
}

void overmap_special::load( const JsonObject &jo, const std::string &src )
{
    const bool strict = is_strict_enabled( src );
    // city_building is just an alias of overmap_special
    // TODO: This comparison is a hack. Separate them properly.
    const bool is_special = jo.get_string( "type", "" ) == "overmap_special";
    const overmap_special_subtype old_type = was_loaded ? subtype_ : overmap_special_subtype::last;

    optional( jo, was_loaded, "subtype", subtype_, overmap_special_subtype::fixed );
    optional( jo, was_loaded, "locations", default_locations_ );
    optional( jo, was_loaded, "connections", connections );

    switch( subtype_ ) {
        case overmap_special_subtype::fixed: {
            shared_ptr_fast<fixed_overmap_special_data> fixed_data;
            if( was_loaded && old_type == subtype_ ) {
                auto data = std::dynamic_pointer_cast<const fixed_overmap_special_data>( data_ );
                fixed_data = make_shared_fast<fixed_overmap_special_data>( *data );
            } else {
                fixed_data = make_shared_fast<fixed_overmap_special_data>();
            }

            mandatory( jo, was_loaded, "overmaps", fixed_data->terrains );

            data_ = std::move( fixed_data );
            break;
        }
        case overmap_special_subtype::mutable_: {
            shared_ptr_fast<mutable_overmap_special_data> mutable_data;
            if( was_loaded && old_type == subtype_ ) {
                auto data = std::dynamic_pointer_cast<const mutable_overmap_special_data>( data_ );
                mutable_data = make_shared_fast<mutable_overmap_special_data>( *data );
            } else {
                mutable_data = make_shared_fast<mutable_overmap_special_data>();
            }

            optional( jo, was_loaded, "check_for_locations", mutable_data->check_for_locations );
            for( JsonObject joc : jo.get_array( "check_for_locations_area" ) ) {
                cata::flat_set<overmap_location_id> type;
                tripoint from;
                tripoint to;
                mandatory( joc, false, "type", type );
                mandatory( joc, false, "from", from );
                mandatory( joc, false, "to", to );
                if( from.x > to.x ) {
                    std::swap( from.x, to.x );
                }
                if( from.y > to.y ) {
                    std::swap( from.y, to.y );
                }
                if( from.z > to.z ) {
                    std::swap( from.z, to.z );
                }
                for( int x = from.x; x <= to.x; x++ ) {
                    for( int y = from.y; y <= to.y; y++ ) {
                        for( int z = from.z; z <= to.z; z++ ) {
                            overmap_special_locations loc;
                            loc.p = tripoint( x, y, z );
                            loc.locations = type;
                            mutable_data->check_for_locations.push_back( loc );
                        }
                    }
                }
            }
            optional( jo, was_loaded, "shared", mutable_data->shared_dist );
            mandatory( jo, was_loaded, "joins", mutable_data->joins_vec );
            mandatory( jo, was_loaded, "overmaps", mutable_data->overmaps );
            mandatory( jo, was_loaded, "root", mutable_data->root );
            mandatory( jo, was_loaded, "phases", mutable_data->phases );

            data_ = std::move( mutable_data );
            break;
        }
        default:
            jo.throw_error( string_format( "subtype %s not implemented",
                                           io::enum_to_string( subtype_ ) ) );
    }

    if( jo.has_array( "place_nested" ) ) {
        nested_.clear();
        JsonArray jar = jo.get_array( "place_nested" );
        while( jar.has_more() ) {
            JsonObject joc = jar.next_object();
            std::pair<tripoint_rel_omt, overmap_special_id> nested;
            mandatory( joc, false, "point", nested.first );
            mandatory( joc, false, "special", nested.second );
            nested_.insert( nested );
        }
    }

    if( is_special ) {
        mandatory( jo, was_loaded, "occurrences", constraints_.occurrences );

        assign( jo, "city_sizes", constraints_.city_size, strict );
        assign( jo, "city_distance", constraints_.city_distance, strict );
    }

    assign( jo, "spawns", monster_spawns_, strict );

    assign( jo, "rotate", rotatable_, strict );
    assign( jo, "flags", flags_, strict );

    // Another hack
    if( !is_special ) {
        flags_.insert( "ELECTRIC_GRID" );
    }
}

void overmap_special::finalize()
{
    const_cast<overmap_special_data &>( *data_ ).finalize(
        "overmap special " + id.str(), default_locations_ );

    for( auto &elem : connections ) {
        elem.finalize();
    }
}

void overmap_special::finalize_mapgen_parameters()
{
    // Extract all the map_special-scoped params from the constituent terrains
    // and put them here
    std::string context = string_format( "overmap_special %s", id.str() );
    for( oter_str_id &t : all_terrains() ) {
        std::string mapgen_id = t->get_mapgen_id();
        mapgen_params_.check_and_merge( get_map_special_params( mapgen_id ), context );
    }
}

void overmap_special::check() const
{
    data_->check( string_format( "overmap special %s", id.str() ) );

    for( const auto &elem : connections ) {
        const oter_str_id &ter = get_terrain_at( elem.p );
        if( !ter.is_null() ) {
            debugmsg( "In overmap special \"%s\", connection at %s overwrites terrain.",
                      id, elem.p.to_string() );
        }
        if( !elem.connection.is_valid() ) {
            debugmsg( "In overmap special \"%s\", connection at %s has invalid id \"%s\".",
                      id, elem.p.to_string(), elem.connection );
        }

        if( elem.from ) {
            // The only supported directions are north/east/south/west
            // as those are the four directions that overmap connections
            // can be made in. If the direction we figured out wasn't
            // one of those, warn the user/developer.
            const direction calculated_direction = direction_from( *elem.from, elem.p );
            switch( calculated_direction ) {
                case direction::NORTH:
                case direction::EAST:
                case direction::SOUTH:
                case direction::WEST:
                    continue;
                default:
                    debugmsg( "In overmap special \"%s\", connection %s is not directly north, east, south or west of the defined \"from\" %s.",
                              id, elem.p.to_string(), elem.from->to_string() );
                    break;
            }
        }
    }

    // Make sure nested specials doesn't loop back to parents
    std::function<std::optional<overmap_special_id>( const overmap_special_id, std::unordered_set<overmap_special_id> ) >
    check_recursion = [&check_recursion]( const overmap_special_id special,
    std::unordered_set<overmap_special_id> parents ) -> std::optional<overmap_special_id> {
        for( const auto &nested : special->get_nested_specials() )
        {
            if( parents.contains( nested.second ) ) {
                return nested.second;
            } else {
                std::unordered_set<overmap_special_id> copy = parents;
                copy.insert( nested.second );
                std::optional<overmap_special_id> recures = check_recursion( nested.second, copy );
                if( recures ) {
                    return *recures;
                }
            }
        }
        return std::nullopt;
    };
    std::unordered_set<overmap_special_id> parents{ id };
    std::optional<overmap_special_id> recurse = check_recursion( id, parents );
    if( recurse ) {
        debugmsg( "In overmap special \"%s\", nested special \"%s\" places itself recursively.",
                  id, *recurse );
    }
}

// *** BEGIN overmap FUNCTIONS ***
overmap::overmap( const point_abs_om &p ) : loc( p )
{
    const std::string rsettings_id = get_option<std::string>( "DEFAULT_REGION" );
    t_regional_settings_map_citr rsit = region_settings_map.find( rsettings_id );

    if( rsit == region_settings_map.end() ) {
        // gonna die now =[
        debugmsg( "overmap %s: can't find region '%s'", loc.to_string(), rsettings_id.c_str() );
    }
    settings = &rsit->second;

    init_layers();
}

overmap::overmap( overmap && )  noexcept = default;
overmap::~overmap() = default;

void overmap::populate( overmap_special_batch &enabled_specials )
{
    try {
        open( enabled_specials );
    } catch( const std::exception &err ) {
        debugmsg( "overmap %s failed to load: %s", loc.to_string(), err.what() );
    }
}

void overmap::populate()
{
    overmap_special_batch enabled_specials = overmap_specials::get_default_batch( loc );
    const overmap_feature_flag_settings &overmap_feature_flag = settings->overmap_feature_flag;

    const bool should_blacklist = !overmap_feature_flag.blacklist.empty();
    const bool should_whitelist = !overmap_feature_flag.whitelist.empty();

    // If this region's settings has blacklisted or whitelisted overmap feature flags, let's
    // filter our default batch.

    // Remove any items that have a flag that is present in the blacklist.
    if( should_blacklist ) {
        for( auto it = enabled_specials.begin(); it != enabled_specials.end(); ) {
            if( cata::sets_intersect( overmap_feature_flag.blacklist,
                                      it->special_details->get_flags() ) ) {
                it = enabled_specials.erase( it );
            } else {
                ++it;
            }
        }
    }

    // Remove any items which do not have any of the flags from the whitelist.
    if( should_whitelist ) {
        for( auto it = enabled_specials.begin(); it != enabled_specials.end(); ) {
            if( cata::sets_intersect( overmap_feature_flag.whitelist,
                                      it->special_details->get_flags() ) ) {
                ++it;
            } else {
                it = enabled_specials.erase( it );
            }
        }
    }

    populate( enabled_specials );
}

oter_id overmap::get_default_terrain( int z ) const
{
    if( z == 0 ) {
        return settings->default_oter.id();
    } else {
        // // TODO: Get rid of the hard-coded ids.
        static const oter_str_id open_air( "open_air" );
        static const oter_str_id empty_rock( "empty_rock" );

        return z > 0 ? open_air.id() : empty_rock.id();
    }
}

void overmap::init_layers()
{
    for( int k = 0; k < OVERMAP_LAYERS; ++k ) {
        const oter_id tid = get_default_terrain( k - OVERMAP_DEPTH );

        for( int i = 0; i < OMAPX; ++i ) {
            for( int j = 0; j < OMAPY; ++j ) {
                layer[k].terrain[i][j] = tid;
                layer[k].visible[i][j] = false;
                layer[k].explored[i][j] = false;
                layer[k].path[i][j] = false;
            }
        }
    }
}

void overmap::ter_set( const tripoint_om_omt &p, const oter_id &id )
{
    if( !inbounds( p ) ) {
        /// TODO: Add a debug message reporting this, but currently there are way too many place that would trigger it.
        return;
    }

    layer[p.z() + OVERMAP_DEPTH].terrain[p.x()][p.y()] = id;
}

const oter_id &overmap::ter( const tripoint_om_omt &p ) const
{
    if( !inbounds( p ) ) {
        /// TODO: Add a debug message reporting this, but currently there are way too many place that would trigger it.
        return ot_null;
    }

    return layer[p.z() + OVERMAP_DEPTH].terrain[p.x()][p.y()];
}

std::string *overmap::join_used_at( const om_pos_dir &p )
{
    auto it = joins_used.find( p );
    if( it == joins_used.end() ) {
        return nullptr;
    }
    return &it->second;
}

std::optional<mapgen_arguments> *overmap::mapgen_args( const tripoint_om_omt &p )
{
    auto it = mapgen_args_index.find( p );
    if( it == mapgen_args_index.end() ) {
        return nullptr;
    }
    return &mapgen_arg_storage[it->second];
}

bool &overmap::seen( const tripoint_om_omt &p )
{
    if( !inbounds( p ) ) {
        nullbool = false;
        return nullbool;
    }
    return layer[p.z() + OVERMAP_DEPTH].visible[p.x()][p.y()];
}

bool overmap::seen( const tripoint_om_omt &p ) const
{
    if( !inbounds( p ) ) {
        return false;
    }
    return layer[p.z() + OVERMAP_DEPTH].visible[p.x()][p.y()];
}

bool &overmap::explored( const tripoint_om_omt &p )
{
    if( !inbounds( p ) ) {
        nullbool = false;
        return nullbool;
    }
    return layer[p.z() + OVERMAP_DEPTH].explored[p.x()][p.y()];
}

bool overmap::is_explored( const tripoint_om_omt &p ) const
{
    if( !inbounds( p ) ) {
        return false;
    }
    return layer[p.z() + OVERMAP_DEPTH].explored[p.x()][p.y()];
}

bool &overmap::path( const tripoint_om_omt &p )
{
    if( !inbounds( p ) ) {
        nullbool = false;
        return nullbool;
    }
    return layer[p.z() + OVERMAP_DEPTH].path[p.x()][p.y()];
}

bool overmap::is_path( const tripoint_om_omt &p ) const
{
    if( !inbounds( p ) ) {
        return false;
    }
    return layer[p.z() + OVERMAP_DEPTH].path[p.x()][p.y()];
}

bool overmap::mongroup_check( const mongroup &candidate ) const
{
    const auto matching_range = zg.equal_range( candidate.pos );
    return std::find_if( matching_range.first, matching_range.second,
    [candidate]( const std::pair<tripoint_om_sm, mongroup> &match ) {
        // This is extra strict since we're using it to test serialization.
        return candidate.type == match.second.type && candidate.pos == match.second.pos &&
               candidate.radius == match.second.radius &&
               candidate.population == match.second.population &&
               candidate.target == match.second.target &&
               candidate.interest == match.second.interest &&
               candidate.dying == match.second.dying &&
               candidate.horde == match.second.horde &&
               candidate.diffuse == match.second.diffuse;
    } ) != matching_range.second;
}

bool overmap::monster_check( const std::pair<tripoint_om_sm, monster> &candidate ) const
{
    const auto matching_range = monster_map->equal_range( candidate.first );
    return std::find_if( matching_range.first, matching_range.second,
    [candidate]( const std::pair<tripoint_om_sm, monster> &match ) {
        return candidate.second.pos() == match.second.pos() &&
               candidate.second.type == match.second.type;
    } ) != matching_range.second;
}

void overmap::insert_npc( const shared_ptr_fast<npc> &who )
{
    npcs.push_back( who );
    g->set_npcs_dirty();
}

shared_ptr_fast<npc> overmap::erase_npc( const character_id &id )
{
    const auto iter = std::find_if( npcs.begin(),
    npcs.end(), [id]( const shared_ptr_fast<npc> &n ) {
        return n->getID() == id;
    } );
    if( iter == npcs.end() ) {
        return nullptr;
    }
    auto ptr = *iter;
    npcs.erase( iter );
    g->set_npcs_dirty();
    return ptr;
}

std::vector<shared_ptr_fast<npc>> overmap::get_npcs( const
                               std::function<bool( const npc & )>
                               &predicate ) const
{
    std::vector<shared_ptr_fast<npc>> result;
    for( const auto &g : npcs ) {
        if( predicate( *g ) ) {
            result.push_back( g );
        }
    }
    return result;
}

bool overmap::has_note( const tripoint_om_omt &p ) const
{
    if( p.z() < -OVERMAP_DEPTH || p.z() > OVERMAP_HEIGHT ) {
        return false;
    }

    for( const om_note &i : layer[p.z() + OVERMAP_DEPTH].notes ) {
        if( i.p == p.xy() ) {
            return true;
        }
    }
    return false;
}

std::optional<int> overmap::has_note_with_danger_radius( const tripoint_om_omt &p ) const
{
    if( p.z() < -OVERMAP_DEPTH || p.z() > OVERMAP_HEIGHT ) {
        return std::nullopt;
    }

    for( const om_note &i : layer[p.z() + OVERMAP_DEPTH].notes ) {
        if( i.p == p.xy() ) {
            if( i.dangerous ) {
                return i.danger_radius;
            } else {
                break;
            }
        }
    }
    return std::nullopt;
}

bool overmap::is_marked_dangerous( const tripoint_om_omt &p ) const
{
    for( const om_note &i : layer[p.z() + OVERMAP_DEPTH].notes ) {
        if( !i.dangerous ) {
            continue;
        } else if( p.xy() == i.p ) {
            return true;
        }
        const int radius = i.danger_radius;
        if( i.danger_radius == 0 && i.p != p.xy() ) {
            continue;
        }
        for( int x = -radius; x <= radius; x++ ) {
            for( int y = -radius; y <= radius; y++ ) {
                const tripoint_om_omt rad_point = tripoint_om_omt( i.p, p.z() ) + point( x, y );
                if( p.xy() == rad_point.xy() ) {
                    return true;
                }
            }
        }
    }
    return false;
}

const std::vector<om_note> &overmap::all_notes( int z ) const
{
    static const std::vector<om_note> fallback;

    if( z < -OVERMAP_DEPTH || z > OVERMAP_HEIGHT ) {
        return fallback;
    }

    return layer[z + OVERMAP_DEPTH].notes;
}

const std::string &overmap::note( const tripoint_om_omt &p ) const
{
    static const std::string fallback {};

    const auto &notes = all_notes( p.z() );
    const auto it = std::find_if( begin( notes ), end( notes ), [&]( const om_note & n ) {
        return n.p == p.xy();
    } );

    return ( it != std::end( notes ) ) ? it->text : fallback;
}

void overmap::add_note( const tripoint_om_omt &p, std::string message )
{
    if( p.z() < -OVERMAP_DEPTH || p.z() > OVERMAP_HEIGHT ) {
        debugmsg( "Attempting to add not to overmap for blank layer %d", p.z() );
        return;
    }

    auto &notes = layer[p.z() + OVERMAP_DEPTH].notes;
    const auto it = std::find_if( begin( notes ), end( notes ), [&]( const om_note & n ) {
        return n.p == p.xy();
    } );

    if( it == std::end( notes ) ) {
        notes.emplace_back( om_note{ std::move( message ), p.xy() } );
    } else if( !message.empty() ) {
        it->text = std::move( message );
    } else {
        notes.erase( it );
    }
}

void overmap::mark_note_dangerous( const tripoint_om_omt &p, int radius, bool is_dangerous )
{
    for( auto &i : layer[p.z() + OVERMAP_DEPTH].notes ) {
        if( p.xy() == i.p ) {
            i.dangerous = is_dangerous;
            i.danger_radius = radius;
            return;
        }
    }
}

void overmap::delete_note( const tripoint_om_omt &p )
{
    add_note( p, std::string{} );
}

std::vector<point_abs_omt> overmap::find_notes( const int z, const std::string &text )
{
    std::vector<point_abs_omt> note_locations;
    map_layer &this_layer = layer[z + OVERMAP_DEPTH];
    for( const auto &note : this_layer.notes ) {
        if( match_include_exclude( note.text, text ) ) {
            note_locations.push_back( project_combine( pos(), note.p ) );
        }
    }
    return note_locations;
}

bool overmap::has_extra( const tripoint_om_omt &p ) const
{
    if( p.z() < -OVERMAP_DEPTH || p.z() > OVERMAP_HEIGHT ) {
        return false;
    }

    for( auto &i : layer[p.z() + OVERMAP_DEPTH].extras ) {
        if( i.p == p.xy() ) {
            return true;
        }
    }
    return false;
}

const string_id<map_extra> &overmap::extra( const tripoint_om_omt &p ) const
{
    static const string_id<map_extra> fallback{};

    if( p.z() < -OVERMAP_DEPTH || p.z() > OVERMAP_HEIGHT ) {
        return fallback;
    }

    const auto &extras = layer[p.z() + OVERMAP_DEPTH].extras;
    const auto it = std::find_if( begin( extras ),
    end( extras ), [&]( const om_map_extra & n ) {
        return n.p == p.xy();
    } );

    return ( it != std::end( extras ) ) ? it->id : fallback;
}

void overmap::add_extra( const tripoint_om_omt &p, const string_id<map_extra> &id )
{
    if( p.z() < -OVERMAP_DEPTH || p.z() > OVERMAP_HEIGHT ) {
        debugmsg( "Attempting to add not to overmap for blank layer %d", p.z() );
        return;
    }

    auto &extras = layer[p.z() + OVERMAP_DEPTH].extras;
    const auto it = std::find_if( begin( extras ),
    end( extras ), [&]( const om_map_extra & n ) {
        return n.p == p.xy();
    } );

    if( it == std::end( extras ) ) {
        extras.emplace_back( om_map_extra{ id, p.xy() } );
    } else if( !id.is_null() ) {
        it->id = id;
    } else {
        extras.erase( it );
    }
}

void overmap::delete_extra( const tripoint_om_omt &p )
{
    add_extra( p, string_id<map_extra>::NULL_ID() );
}

std::vector<point_abs_omt> overmap::find_extras( const int z, const std::string &text )
{
    std::vector<point_abs_omt> extra_locations;
    map_layer &this_layer = layer[z + OVERMAP_DEPTH];
    for( const auto &extra : this_layer.extras ) {
        const std::string extra_text = extra.id.c_str();
        if( match_include_exclude( extra_text, text ) ) {
            extra_locations.push_back( project_combine( pos(), extra.p ) );
        }
    }
    return extra_locations;
}

bool overmap::inbounds( const tripoint_om_omt &p, int clearance )
{
    static constexpr tripoint_om_omt overmap_boundary_min( 0, 0, -OVERMAP_DEPTH );
    static constexpr tripoint_om_omt overmap_boundary_max( OMAPX, OMAPY, OVERMAP_HEIGHT + 1 );

    static constexpr half_open_cuboid<tripoint_om_omt> overmap_boundaries(
        overmap_boundary_min, overmap_boundary_max );
    half_open_cuboid<tripoint_om_omt> stricter_boundaries = overmap_boundaries;
    stricter_boundaries.shrink( tripoint( clearance, clearance, 0 ) );

    return stricter_boundaries.contains( p );
}

const scent_trace &overmap::scent_at( const tripoint_abs_omt &loc ) const
{
    static const scent_trace null_scent;
    const auto &scent_found = scents.find( loc );
    if( scent_found != scents.end() ) {
        return scent_found->second;
    }
    return null_scent;
}

void overmap::set_scent( const tripoint_abs_omt &loc, const scent_trace &new_scent )
{
    // TODO: increase strength of scent trace when applied repeatedly in a short timespan.
    scents[loc] = new_scent;
}

void overmap::generate( const overmap *north, const overmap *east,
                        const overmap *south, const overmap *west,
                        overmap_special_batch &enabled_specials )
{
    if( g->gametype() == special_game_type::DEFENSE ) {
        dbg( DL::Info ) << "overmap::generate skipped in Defense special game mode!";
        return;
    }

    dbg( DL::Info ) << "overmap::generate start";

    connection_cache = overmap_connection_cache{};
    populate_connections_out_from_neighbors( north, east, south, west );

    place_rivers( north, east, south, west );
    place_lakes();
    place_forests();
    place_swamps();
    place_cities();
    place_forest_trails();
    place_roads( north, east, south, west );
    place_specials( enabled_specials );
    place_forest_trailheads();

    polish_rivers( north, east, south, west );

    // Place the monsters, now that the terrain is laid out
    place_mongroups();
    place_radios();

    connection_cache.reset();

    dbg( DL::Info ) << "overmap::generate done";
}

static void elevate_bridges(
    overmap &om,
    const std::vector<point_om_omt> &bridge_points,
    const std::string &bridge_overpass_id,
    const std::string &bridge_under_id,
    const std::string &bridgehead_ground_id,
    const std::string &bridgehead_ramp_id,
    const std::string &bridge_center_id,
    const std::string &bridge_flat_ew_id,
    const std::string &bridge_flat_ns_id
)
{
    // Check bridgeheads and 1-tile-long bridges
    std::vector<std::pair<point_om_omt, om_direction::type>> bridgehead_points;
    std::set<point_om_omt> flatten_points;
    std::pair<point_om_omt, om_direction::type> center_point;
    int spot = 0;
    const int len = std::size( bridge_points );
    for( const point_om_omt &bp : bridge_points ) {
        tripoint_om_omt bp_om( bp, 0 );

        const oter_id &ot_here = om.ter( bp_om );
        const std::string &type_here = ot_here->get_type_id().str();
        const om_direction::type dir = oter_get_rotation_dir( ot_here );
        if( dir == om_direction::type::invalid ) {
            // Shouldn't happen
            debugmsg( "Potential bridgehead %s at %s has invalid rotation.", ot_here.id(), bp_om.to_string() );
            continue;
        }
        point vec = om_direction::displace( dir );
        const bool is_bridge_fwd = is_ot_match( type_here, om.ter( bp_om + vec ), ot_match_type::type );
        const bool is_bridge_bck = is_ot_match( type_here, om.ter( bp_om - vec ), ot_match_type::type );

        if( is_bridge_fwd ^ is_bridge_bck ) {
            om_direction::type ramp_facing = is_bridge_fwd ? om_direction::opposite( dir ) : dir;
            bridgehead_points.emplace_back( bp, ramp_facing );
        } else if( !is_bridge_fwd && !is_bridge_bck ) {
            flatten_points.emplace( bp );
        } else if( ( len / 2 == spot ) ) {
            center_point = { bp, dir };
        }
        spot++;
    }
    // Flatten 1-tile-long bridges
    for( const point_om_omt &bp : flatten_points ) {
        tripoint_om_omt p( bp, 0 );
        om_direction::type rot = oter_get_rotation_dir( om.ter( p ) );
        if( rot == om_direction::type::east || rot == om_direction::type::west ) {
            om.ter_set( p, oter_id( bridge_flat_ew_id ) );
        } else {
            om.ter_set( p, oter_id( bridge_flat_ns_id ) );
        }
    }
    // Put bridge points
    for( const point_om_omt &bp : bridge_points ) {
        if( flatten_points.contains( bp ) ) {
            continue;
        }
        tripoint_om_omt p( bp, 0 );
        const std::string &rot_sfx = oter_get_rotation_string( om.ter( p ) );
        om.ter_set( p + tripoint_above, oter_id( bridge_overpass_id + rot_sfx ) );
        om.ter_set( p, oter_id( bridge_under_id + rot_sfx ) );
    }
    // Put bridgeheads
    for( const std::pair<point_om_omt, om_direction::type> &bhp : bridgehead_points ) {
        tripoint_om_omt p( bhp.first, 0 );
        const std::string &dir_suffix = om_direction::all_suffixes[static_cast<int>( bhp.second )];
        om.ter_set( p, oter_id( bridgehead_ground_id + dir_suffix ) );
        om.ter_set( p + tripoint_above, oter_id( bridgehead_ramp_id + dir_suffix ) );
    }
    if( !bridge_center_id.empty() ) {
        tripoint_om_omt p( center_point.first, 0 );
        om.ter_set( p, oter_id( bridge_center_id + om_direction::all_suffixes[static_cast<int>
                                ( center_point.second )] ) );
    }
}

std::vector<point_abs_omt> overmap::find_terrain( const std::string &term, int zlevel )
{
    std::vector<point_abs_omt> found;
    for( int x = 0; x < OMAPX; x++ ) {
        for( int y = 0; y < OMAPY; y++ ) {
            tripoint_om_omt p( x, y, zlevel );
            if( seen( p ) &&
                lcmatch( ter( p )->get_name(), term ) ) {
                found.push_back( project_combine( pos(), p.xy() ) );
            }
        }
    }
    return found;
}

const city &overmap::get_nearest_city( const tripoint_om_omt &p ) const
{
    int distance = 999;
    const city *res = nullptr;
    for( const auto &elem : cities ) {
        const int dist = elem.get_distance_from( p );
        if( dist < distance ) {
            distance = dist;
            res = &elem;
        }
    }
    if( res != nullptr ) {
        return *res;
    }
    static city invalid_city;
    return invalid_city;
}

tripoint_om_omt overmap::find_random_omt( const std::pair<std::string, ot_match_type> &target )
const
{
    std::vector<tripoint_om_omt> valid;
    for( int i = 0; i < OMAPX; i++ ) {
        for( int j = 0; j < OMAPY; j++ ) {
            for( int k = -OVERMAP_DEPTH; k <= OVERMAP_HEIGHT; k++ ) {
                tripoint_om_omt p( i, j, k );
                if( is_ot_match( target.first, ter( p ), target.second ) ) {
                    valid.push_back( p );
                }
            }
        }
    }
    return random_entry( valid, tripoint_om_omt( tripoint_min ) );
}

void overmap::process_mongroups()
{
    for( auto it = zg.begin(); it != zg.end(); ) {
        mongroup &mg = it->second;
        if( mg.dying ) {
            mg.population = ( mg.population * 4 ) / 5;
            mg.radius = ( mg.radius * 9 ) / 10;
        }
        if( mg.empty() ) {
            zg.erase( it++ );
        } else {
            ++it;
        }
    }
}

void overmap::clear_mon_groups()
{
    zg.clear();
}

void overmap::clear_overmap_special_placements()
{
    overmap_special_placements.clear();
}
void overmap::clear_cities()
{
    cities.clear();
}
void overmap::clear_connections_out()
{
    connections_out.clear();
}

static std::map<std::string, std::string> oter_id_migrations;

void overmap::load_oter_id_migration( const JsonObject &jo )
{
    const bool old_directions = jo.get_bool( "old_directions", false );
    const bool new_directions = jo.get_bool( "new_directions", false );

    for( const JsonMember &kv : jo.get_object( "oter_ids" ) ) {
        if( old_directions ) {
            for( const std::string &suffix : om_direction::all_suffixes ) {
                oter_id_migrations.emplace( kv.name() + suffix,
                                            new_directions ? kv.get_string() + suffix : kv.get_string() );
            }
        } else {
            oter_id_migrations.emplace( kv.name(), kv.get_string() );
        }
    }
}

void overmap::reset_oter_id_migrations()
{
    oter_id_migrations.clear();
}

bool overmap::is_oter_id_obsolete( const std::string &oterid )
{
    return oter_id_migrations.contains( oterid );
}

void overmap::migrate_oter_ids( const std::unordered_map<tripoint_om_omt, std::string> &points )
{
    for( const auto&[pos, old_id] : points ) {
        const oter_str_id new_id = oter_str_id( oter_id_migrations.at( old_id ) );
        const tripoint_abs_sm pos_abs = project_to<coords::sm>( project_combine( this->pos(), pos ) );

        if( new_id.is_valid() ) {
            DebugLog( DL::Warn, DC::Map ) << "migrated oter_id '" << old_id << "' at " << pos_abs
                                          << " to '" << new_id.str() << "'";

            ter_set( pos, new_id );
        } else {
            debugmsg( "oter_id migration defined from '%s' to invalid ter_id '%s'", old_id, new_id.str() );
        }
    }
}

void overmap::place_special_forced( const overmap_special_id &special_id, const tripoint_om_omt &p,
                                    om_direction::type dir )
{
    static city invalid_city;
    place_special( *special_id, p, dir, invalid_city, false, true );
}

void mongroup::wander( const overmap &om )
{
    const city *target_city = nullptr;
    int target_distance = 0;

    if( horde_behaviour == "city" ) {
        // Find a nearby city to return to..
        for( const city &check_city : om.cities ) {
            // Check if this is the nearest city so far.
            int distance = rl_dist( project_to<coords::sm>( check_city.pos ),
                                    pos.xy() );
            if( !target_city || distance < target_distance ) {
                target_distance = distance;
                target_city = &check_city;
            }
        }
    }

    if( target_city ) {
        // TODO: somehow use the same algorithm that distributes zombie
        // density at world gen to spread the hordes over the actual
        // city, rather than the center city tile
        target.x() = target_city->pos.x() * 2 + rng( -target_city->size * 2, target_city->size * 2 );
        target.y() = target_city->pos.y() * 2 + rng( -target_city->size * 2, target_city->size * 2 );
        interest = 100;
    } else {
        target.x() = pos.x() + rng( -10, 10 );
        target.y() = pos.y() + rng( -10, 10 );
        interest = 30;
    }
}

void overmap::move_hordes()
{
    // Prevent hordes to be moved twice by putting them in here after moving.
    decltype( zg ) tmpzg;
    //MOVE ZOMBIE GROUPS
    for( auto it = zg.begin(); it != zg.end(); ) {
        mongroup &mg = it->second;
        if( !mg.horde || mg.horde_behaviour == "nemesis" ) {
            // Nemesis hordes have their own move logic.
            ++it;
            continue;
        }

        if( mg.horde_behaviour.empty() ) {
            mg.horde_behaviour = one_in( 2 ) ? "city" : "roam";
        }

        // Gradually decrease interest.
        mg.dec_interest( 1 );

        if( ( mg.pos.xy() == mg.target.xy() ) || mg.interest <= 15 ) {
            mg.wander( *this );
        }

        // Decrease movement chance according to the terrain we're currently on.
        const oter_id &walked_into = ter( project_to<coords::omt>( mg.pos ) );
        int movement_chance = 1;
        if( walked_into == ot_forest || walked_into == ot_forest_water ) {
            movement_chance = 3;
        } else if( walked_into == ot_forest_thick ) {
            movement_chance = 6;
        } else if( walked_into == ot_river_center ) {
            movement_chance = 10;
        }

        // If the average horde speed is 50% that of normal, then the chance to
        // move should be 1/2 what it would be if the speed was 100%.
        // Since the max speed for a horde is one map space per 2.5 minutes,
        // choose that to be the speed of the fastest horde monster, which is
        // roughly 200 at the time of writing. So a horde with average speed
        // 200 or over will move at max speed, and slower hordes will move less
        // frequently. The average horde speed for regular Z's is around 100,
        // or one space per 5 minutes.
        if( one_in( movement_chance ) && rng( 0, 100 ) < mg.interest && rng( 0, 200 ) < mg.avg_speed() ) {
            // TODO: Handle moving to adjacent overmaps.
            if( mg.pos.x() > mg.target.x() ) {
                mg.pos.x()--;
            }
            if( mg.pos.x() < mg.target.x() ) {
                mg.pos.x()++;
            }
            if( mg.pos.y() > mg.target.y() ) {
                mg.pos.y()--;
            }
            if( mg.pos.y() < mg.target.y() ) {
                mg.pos.y()++;
            }

            // Erase the group at it's old location, add the group with the new location
            tmpzg.insert( std::pair<tripoint_om_sm, mongroup>( mg.pos, mg ) );
            zg.erase( it++ );
        } else {
            ++it;
        }
    }
    // and now back into the monster group map.
    zg.insert( tmpzg.begin(), tmpzg.end() );

    if( get_option<bool>( "WANDER_SPAWNS" ) ) {

        // Re-absorb zombies into hordes.
        // Scan over monsters outside the player's view and place them back into hordes.
        auto monster_map_it = monster_map->begin();
        while( monster_map_it != monster_map->end() ) {
            const auto &p = monster_map_it->first;
            auto &this_monster = monster_map_it->second;

            // Only zombies on z-level 0 may join hordes.
            if( p.z() != 0 ) {
                monster_map_it++;
                continue;
            }

            // Check if the monster is a zombie.
            auto &type = *( this_monster.type );
            if(
                !type.species.contains( ZOMBIE ) || // Only add zombies to hordes.
                type.id == mtype_id( "mon_jabberwock" ) || // Jabberwockies are an exception.
                this_monster.get_speed() <= 30 || // So are very slow zombies, like crawling zombies.
                this_monster.has_flag( MF_IMMOBILE ) || // Also exempt anything stationary.
                this_monster.has_effect( effect_pet ) || // "Zombie pet" zlaves are, too.
                !this_monster.will_join_horde( INT_MAX ) || // So are zombies who won't join a horde of any size.
                this_monster.mission_id != -1 // We mustn't delete monsters that are related to missions.
            ) {
                // Don't delete the monster, just increment the iterator.
                monster_map_it++;
                continue;
            }

            // Scan for compatible hordes in this area, selecting the largest.
            mongroup *add_to_group = nullptr;
            auto group_bucket = zg.equal_range( p );
            std::vector<monster>::size_type add_to_horde_size = 0;
            std::for_each( group_bucket.first, group_bucket.second,
            [&]( std::pair<const tripoint_om_sm, mongroup> &horde_entry ) {
                mongroup &horde = horde_entry.second;

                // We only absorb zombies into GROUP_ZOMBIE hordes
                if( horde.horde && !horde.monsters.empty() && horde.type == GROUP_ZOMBIE &&
                    horde.monsters.size() > add_to_horde_size ) {
                    add_to_group = &horde;
                    add_to_horde_size = horde.monsters.size();
                }
            } );

            // Check again if the zombie will join the largest horde, now that we know the accurate size.
            if( this_monster.will_join_horde( add_to_horde_size ) ) {
                // If there is no horde to add the monster to, create one.
                if( add_to_group == nullptr ) {
                    mongroup m( GROUP_ZOMBIE, p, 1, 0 );
                    m.horde = true;
                    m.monsters.push_back( this_monster );
                    m.interest = 0; // Ensures that we will select a new target.
                    add_mon_group( m );
                } else {
                    add_to_group->monsters.push_back( this_monster );
                }
            } else { // Bad luck--the zombie would have joined a larger horde, but not this one.  Skip.
                // Don't delete the monster, just increment the iterator.
                monster_map_it++;
                continue;
            }

            // Delete the monster, continue iterating.
            monster_map_it = monster_map->erase( monster_map_it );
        }
    }
}

void overmap::move_nemesis()
{
    // Prevent hordes to be moved twice by putting them in here after moving.
    decltype( zg ) tmpzg;
    for( std::multimap<tripoint_om_sm, mongroup>::iterator it = zg.begin(); it != zg.end(); ) {
        mongroup &mg = it->second;
        if( !mg.horde || mg.horde_behaviour != "nemesis" ) {
            ++it;
            continue;
        }

        // Decrease movement chance according to the terrain we're currently on.
        const oter_id &walked_into = ter( project_to<coords::omt>( mg.pos ) );
        int movement_chance = 1;
        if( walked_into == ot_forest || walked_into == ot_forest_water ) {
            movement_chance = 3;
        } else if( walked_into == ot_forest_thick ) {
            movement_chance = 6;
        } else if( walked_into == ot_river_center ) {
            movement_chance = 10;
        }

        if( one_in( movement_chance ) && rng( 0, 200 ) < mg.avg_speed() ) {
            if( mg.abs_pos.x() > mg.nemesis_target.x() ) {
                mg.abs_pos.x()--;
            }
            if( mg.abs_pos.x() < mg.nemesis_target.x() ) {
                mg.abs_pos.x()++;
            }
            if( mg.abs_pos.y() > mg.nemesis_target.y() ) {
                mg.abs_pos.y()--;
            }
            if( mg.abs_pos.y() < mg.nemesis_target.y() ) {
                mg.abs_pos.y()++;
            }

            if( project_to<coords::om>( mg.abs_pos ) == project_to<coords::om>( mg.nemesis_target ) ) {
                point_abs_om omp;
                tripoint_om_sm local_sm;
                std::tie( omp, local_sm ) = project_remain<coords::om>( mg.abs_pos );

                mg.pos.y() = local_sm.y();
                mg.pos.x() = local_sm.x();

                // Erase the group at its old location, add the group with the new location
                tmpzg.insert( std::pair<tripoint_om_sm, mongroup>( mg.pos, mg ) );
                zg.erase( it++ );
                break;
            }
        } else {
            break;
        }
        break;
    }
    // and now back into the monster group map.
    zg.insert( tmpzg.begin(), tmpzg.end() );
}

bool overmap::remove_nemesis()
{
    for( std::multimap<tripoint_om_sm, mongroup>::iterator it = zg.begin(); it != zg.end(); ) {
        mongroup &mg = it->second;
        if( mg.horde_behaviour == "nemesis" ) {
            zg.erase( it++ );
            return true;
        }
        ++it;
    }
    return false;
}

/**
* @param p location of signal relative to this overmap origin
* @param sig_power - power of signal or max distance for reaction of zombies
*/
void overmap::signal_hordes( const tripoint_rel_sm &p_rel, const int sig_power )
{
    tripoint_om_sm p( p_rel.raw() );
    for( auto &elem : zg ) {
        mongroup &mg = elem.second;
        if( !mg.horde ) {
            continue;
        }
        if( mg.horde_behaviour == "nemesis" ) {
            // Nemesis hordes are signaled to the player by their own function.
            continue;
        }
        const int dist = rl_dist( p, mg.pos );
        if( sig_power < dist ) {
            continue;
        }
        // TODO: base this in monster attributes, foremost GOODHEARING.
        const int inter_per_sig_power = 15; //Interest per signal value
        const int min_initial_inter = 30; //Min initial interest for horde
        const int calculated_inter = ( sig_power + 1 - dist ) * inter_per_sig_power; // Calculated interest
        const int roll = rng( 0, mg.interest );
        // Minimum capped calculated interest. Used to give horde enough interest to really investigate the target at start.
        const int min_capped_inter = std::max( min_initial_inter, calculated_inter );
        if( roll < min_capped_inter ) { //Rolling if horde interested in new signal
            // TODO: Z-coordinate for mongroup targets
            const int targ_dist = rl_dist( p, mg.target );
            // TODO: Base this on targ_dist:dist ratio.
            if( targ_dist < 5 ) {  // If signal source already pursued by horde
                mg.set_target( midpoint( mg.target.xy(), p.xy() ) );
                const int min_inc_inter = 3; // Min interest increase to already targeted source
                const int inc_roll = rng( min_inc_inter, calculated_inter );
                mg.inc_interest( inc_roll );
                add_msg( m_debug, "horde inc interest %d dist %d", inc_roll, dist );
            } else { // New signal source
                mg.set_target( p.xy() );
                mg.set_interest( min_capped_inter );
                add_msg( m_debug, "horde set interest %d dist %d", min_capped_inter, dist );
            }
        }
    }
}

void overmap::signal_nemesis( const tripoint_abs_sm p_abs_sm )
{
    point_abs_om omp;
    tripoint_om_sm local_sm;
    std::tie( omp, local_sm ) = project_remain<coords::om>( p_abs_sm );
    const point_om_sm pos_om = local_sm.xy();

    for( std::pair<const tripoint_om_sm, mongroup> &elem : zg ) {
        mongroup &mg = elem.second;

        if( mg.horde_behaviour == "nemesis" ) {
            // If the horde is a nemesis, we set its target directly on the player.
            mg.set_target( pos_om );
            mg.set_nemesis_target( p_abs_sm );
        }
    }
}

void overmap::populate_connections_out_from_neighbors( const overmap *north, const overmap *east,
        const overmap *south, const overmap *west )
{
    const auto populate_for_side =
        [&]( const overmap * adjacent,
             const std::function<bool( const tripoint_om_omt & )> &should_include,
    const std::function<tripoint_om_omt( const tripoint_om_omt & )> &build_point ) {
        if( adjacent == nullptr ) {
            return;
        }

        for( const std::pair<const overmap_connection_id, std::vector<tripoint_om_omt>> &kv :
             adjacent->connections_out ) {
            std::vector<tripoint_om_omt> &out = connections_out[kv.first];
            const auto adjacent_out = adjacent->connections_out.find( kv.first );
            if( adjacent_out != adjacent->connections_out.end() ) {
                for( const tripoint_om_omt &p : adjacent_out->second ) {
                    if( should_include( p ) ) {
                        out.push_back( build_point( p ) );
                    }
                }
            }
        }
    };

    populate_for_side( north, []( const tripoint_om_omt & p ) {
        return p.y() == OMAPY - 1;
    }, []( const tripoint_om_omt & p ) {
        return tripoint_om_omt( p.x(), 0, p.z() );
    } );
    populate_for_side( west, []( const tripoint_om_omt & p ) {
        return p.x() == OMAPX - 1;
    }, []( const tripoint_om_omt & p ) {
        return tripoint_om_omt( 0, p.y(), p.z() );
    } );
    populate_for_side( south, []( const tripoint_om_omt & p ) {
        return p.y() == 0;
    }, []( const tripoint_om_omt & p ) {
        return tripoint_om_omt( p.x(), OMAPY - 1, p.z() );
    } );
    populate_for_side( east, []( const tripoint_om_omt & p ) {
        return p.x() == 0;
    }, []( const tripoint_om_omt & p ) {
        return tripoint_om_omt( OMAPX - 1, p.y(), p.z() );
    } );
}

void overmap::place_forest_trails()
{
    const forest_trail_settings &forest_trail = settings->forest_trail;
    std::unordered_set<point_om_omt> visited;

    const auto is_forest = [&]( const point_om_omt & p ) {
        if( !inbounds( p, 1 ) ) {
            return false;
        }
        const auto current_terrain = ter( tripoint_om_omt( p, 0 ) );
        return current_terrain == "forest" || current_terrain == "forest_thick" ||
               current_terrain == "forest_water";
    };

    for( int i = 0; i < OMAPX; i++ ) {
        for( int j = 0; j < OMAPY; j++ ) {
            tripoint_om_omt seed_point( i, j, 0 );

            oter_id oter = ter( seed_point );
            if( !is_ot_match( "forest", oter, ot_match_type::prefix ) ) {
                continue;
            }

            // If we've already visited this point, we don't need to
            // process it since it's already part of another forest.
            if( visited.contains( seed_point.xy() ) ) {
                continue;
            }

            // Get the contiguous forest from this point.
            std::vector<point_om_omt> forest_points =
                ff::point_flood_fill_4_connected( seed_point.xy(), visited, is_forest );

            // If we don't have enough points to build a trail, move on.
            if( forest_points.empty() ||
                forest_points.size() < static_cast<std::vector<point>::size_type>
                ( forest_trail.minimum_forest_size ) ) {
                continue;
            }

            // If we don't rng a forest based on our settings, move on.
            if( !one_in( forest_trail.chance ) ) {
                continue;
            }

            // Get the north and south most points in the forest.
            auto north_south_most = std::minmax_element( forest_points.begin(),
            forest_points.end(), []( const point_om_omt & lhs, const point_om_omt & rhs ) {
                return lhs.y() < rhs.y();
            } );

            // Get the west and east most points in the forest.
            auto west_east_most = std::minmax_element( forest_points.begin(),
            forest_points.end(), []( const point_om_omt & lhs, const point_om_omt & rhs ) {
                return lhs.x() < rhs.x();
            } );

            // We'll use these points later as points that are guaranteed to be
            // at a boundary and will form a good foundation for the trail system.
            point_om_omt northmost = *north_south_most.first;
            point_om_omt southmost = *north_south_most.second;
            point_om_omt westmost = *west_east_most.first;
            point_om_omt eastmost = *west_east_most.second;

            // Do a simplistic calculation of the center of the forest (rather than
            // calculating the actual centroid--it's not that important) to have another
            // good point to form the foundation of the trail system.
            point_om_omt center( westmost.x() + ( eastmost.x() - westmost.x() ) / 2,
                                 northmost.y() + ( southmost.y() - northmost.y() ) / 2 );

            point_om_omt center_point = center;

            // Because we didn't do the centroid of a concave polygon, there's no
            // guarantee that our center point is actually within the bounds of the
            // forest. Just find the point within our set that is closest to our
            // center point and use that.
            point_om_omt actual_center_point =
                *std::min_element( forest_points.begin(), forest_points.end(),
            [&center_point]( const point_om_omt & lhs, const point_om_omt & rhs ) {
                return square_dist( lhs, center_point ) < square_dist( rhs,
                        center_point );
            } );

            // Figure out how many random points we'll add to our trail system, based on the forest
            // size and our configuration.
            int max_random_points = forest_trail.random_point_min + forest_points.size() /
                                    forest_trail.random_point_size_scalar;
            max_random_points = std::min( max_random_points, forest_trail.random_point_max );

            // Start with the center...
            std::vector<point_om_omt> chosen_points = { actual_center_point };

            // ...and then add our random points.
            int random_point_count = 0;
            std::shuffle( forest_points.begin(), forest_points.end(), rng_get_engine() );
            for( auto &random_point : forest_points ) {
                if( random_point_count >= max_random_points ) {
                    break;
                }
                random_point_count++;
                chosen_points.emplace_back( random_point );
            }

            // Add our north/south/west/east-most points based on our configuration.
            if( one_in( forest_trail.border_point_chance ) ) {
                chosen_points.emplace_back( northmost );
            }
            if( one_in( forest_trail.border_point_chance ) ) {
                chosen_points.emplace_back( southmost );
            }
            if( one_in( forest_trail.border_point_chance ) ) {
                chosen_points.emplace_back( westmost );
            }
            if( one_in( forest_trail.border_point_chance ) ) {
                chosen_points.emplace_back( eastmost );
            }

            // Finally, connect all the points and make a forest trail out of them.
            const overmap_connection_id forest_trail( "forest_trail" );
            connect_closest_points( chosen_points, 0, *forest_trail );
        }
    }
}

void overmap::place_forest_trailheads()
{
    // No trailheads if there are no cities.
    const int city_size = get_option<int>( "CITY_SIZE" );
    if( city_size <= 0 ) {
        return;
    }

    // Trailheads may be placed if all of the following are true:
    // 1. we're at a forest_trail_end_north/south/west/east,
    // 2. we're within trailhead_road_distance from an existing road
    // 3. rng rolls a success for our trailhead_chance from the configuration
    // 4. the trailhead special we've picked can be placed in the selected location

    const auto trailhead_close_to_road = [&]( const tripoint_om_omt & trailhead ) {
        bool close = false;
        for( const tripoint_om_omt &nearby_point : closest_points_first(
                 trailhead,
                 settings->forest_trail.trailhead_road_distance
             ) ) {
            if( check_ot( "road", ot_match_type::contains, nearby_point ) ) {
                close = true;
            }
        }
        return close;
    };

    const auto try_place_trailhead_special = [&]( const tripoint_om_omt & trail_end,
    const om_direction::type & dir ) {
        const tripoint_om_omt potential_trailhead = trail_end + om_direction::displace( dir, 1 );
        overmap_special_id trailhead = settings->forest_trail.trailheads.pick();
        if( one_in( settings->forest_trail.trailhead_chance ) &&
            trailhead_close_to_road( potential_trailhead ) &&
            can_place_special( *trailhead, potential_trailhead, dir, false ) ) {
            const city &nearest_city = get_nearest_city( potential_trailhead );
            place_special( *trailhead, potential_trailhead, dir, nearest_city, false, false );
        }
    };

    for( int i = 2; i < OMAPX - 2; i++ ) {
        for( int j = 2; j < OMAPY - 2; j++ ) {
            const tripoint_om_omt p( i, j, 0 );
            oter_id oter = ter( p );
            if( is_ot_match( "forest_trail_end", oter, ot_match_type::prefix ) ) {
                try_place_trailhead_special( p, oter->get_dir() );
            }
        }
    }
}

void overmap::place_forests()
{
    const oter_id default_oter_id( settings->default_oter );
    const oter_id forest( "forest" );
    const oter_id forest_thick( "forest_thick" );

    const om_noise::om_noise_layer_forest f( global_base_point(), g->get_seed() );

    for( int x = 0; x < OMAPX; x++ ) {
        for( int y = 0; y < OMAPY; y++ ) {
            const tripoint_om_omt p( x, y, 0 );
            const oter_id &oter = ter( p );

            // At this point in the process, we only want to consider converting the terrain into
            // a forest if it's currently the default terrain type (e.g. a field).
            if( oter != default_oter_id ) {
                continue;
            }

            const float n = f.noise_at( p.xy() );

            // If the noise here meets our threshold, turn it into a forest.
            if( n > settings->overmap_forest.noise_threshold_forest_thick ) {
                ter_set( p, forest_thick );
            } else if( n > settings->overmap_forest.noise_threshold_forest ) {
                ter_set( p, forest );
            }
        }
    }
}

void overmap::place_lakes()
{
    const om_noise::om_noise_layer_lake f( global_base_point(), g->get_seed() );

    const auto is_lake = [&]( const point_om_omt & p ) {
        return f.noise_at( p ) > settings->overmap_lake.noise_threshold_lake;
    };

    const oter_id lake_surface( "lake_surface" );
    const oter_id lake_shore( "lake_shore" );
    const oter_id lake_underwater_shore( "lake_underwater_shore" );
    const oter_id lake_water_cube( "lake_water_cube" );
    const oter_id lake_bed( "lake_bed" );

    // We'll keep track of our visited lake points so we don't repeat the work.
    std::unordered_set<point_om_omt> visited;

    for( int i = 0; i < OMAPX; i++ ) {
        for( int j = 0; j < OMAPY; j++ ) {
            point_om_omt seed_point( i, j );
            if( visited.contains( seed_point ) ) {
                continue;
            }

            // It's a lake if it exceeds the noise threshold defined in the region settings.
            if( !is_lake( seed_point ) ) {
                continue;
            }

            // We're going to flood-fill our lake so that we can consider the entire lake when evaluating it
            // for placement, even when the lake runs off the edge of the current overmap.
            std::vector<point_om_omt> lake_points =
                ff::point_flood_fill_4_connected( seed_point, visited, is_lake );

            // If this lake doesn't exceed our minimum size threshold, then skip it. We can use this to
            // exclude the tiny lakes that don't provide interesting map features and exist mostly as a
            // noise artifact.
            if( lake_points.size() < static_cast<std::vector<point>::size_type>
                ( settings->overmap_lake.lake_size_min ) ) {
                continue;
            }

            // Build a set of "lake" points. We're actually going to combine both the lake points
            // we just found AND all of the rivers on the map, because we want our lakes to write
            // over any rivers that are placed already. Note that the assumption here is that river
            // overmap generation (e.g. place_rivers) runs BEFORE lake overmap generation.
            std::unordered_set<point_om_omt> lake_set;
            for( auto &p : lake_points ) {
                lake_set.emplace( p );
            }

            for( int x = 0; x < OMAPX; x++ ) {
                for( int y = 0; y < OMAPY; y++ ) {
                    const tripoint_om_omt p( x, y, 0 );
                    if( ter( p )->is_river() ) {
                        lake_set.emplace( p.xy() );
                    }
                }
            }

            // Iterate through all of our lake points, rejecting the ones that are out of bounds. For
            // those that are inbounds, look at the 8 adjacent locations and see if they are also part
            // of our lake points set. If they are, that means that this location is entirely surrounded
            // by lake and should be considered a lake surface. If at least one adjacent location is not
            // part of this lake points set, that means this location should be considered a lake shore.
            // Either way, make the determination and set the overmap terrain.
            for( auto &p : lake_points ) {
                if( !inbounds( p ) ) {
                    continue;
                }

                bool shore = false;
                for( int ni = -1; ni <= 1 && !shore; ni++ ) {
                    for( int nj = -1; nj <= 1 && !shore; nj++ ) {
                        const point_om_omt n = p + point( ni, nj );
                        if( !lake_set.contains( n ) ) {
                            shore = true;
                        }
                    }
                }

                ter_set( tripoint_om_omt( p, 0 ), shore ? lake_shore : lake_surface );

                // If this is not a shore, we'll make our subsurface lake cubes and beds.
                if( !shore ) {
                    for( int z = -1; z > settings->overmap_lake.lake_depth; z-- ) {
                        ter_set( tripoint_om_omt( p, z ), lake_water_cube );
                    }
                    ter_set( tripoint_om_omt( p, settings->overmap_lake.lake_depth ), lake_bed );
                } else {
                    for( int z = -1; z >= settings->overmap_lake.lake_depth; z-- ) {
                        ter_set( tripoint_om_omt( p, z ), lake_underwater_shore );
                    }
                }
            }

            // We're going to attempt to connect some points on this lake to the nearest river.
            const auto connect_lake_to_closest_river =
            [&]( const point_om_omt & lake_connection_point ) {
                int closest_distance = -1;
                point_om_omt closest_point;
                for( int x = 0; x < OMAPX; x++ ) {
                    for( int y = 0; y < OMAPY; y++ ) {
                        const tripoint_om_omt p( x, y, 0 );
                        if( !ter( p )->is_river() ) {
                            continue;
                        }
                        const int distance = square_dist( lake_connection_point, p.xy() );
                        if( distance < closest_distance || closest_distance < 0 ) {
                            closest_point = p.xy();
                            closest_distance = distance;
                        }
                    }
                }

                if( closest_distance > 0 ) {
                    place_river( closest_point, lake_connection_point );
                }
            };

            // Get the north and south most points in our lake.
            auto north_south_most = std::minmax_element( lake_points.begin(), lake_points.end(),
            []( const point_om_omt & lhs, const point_om_omt & rhs ) {
                return lhs.y() < rhs.y();
            } );

            point_om_omt northmost = *north_south_most.first;
            point_om_omt southmost = *north_south_most.second;

            // It's possible that our northmost/southmost points in the lake are not on this overmap, because our
            // lake may extend across multiple overmaps.
            if( inbounds( northmost ) ) {
                connect_lake_to_closest_river( northmost );
            }

            if( inbounds( southmost ) ) {
                connect_lake_to_closest_river( southmost );
            }
        }
    }
}

void overmap::place_rivers( const overmap *north, const overmap *east, const overmap *south,
                            const overmap *west )
{
    if( settings->river_scale == 0.0 ) {
        return;
    }
    int river_chance = static_cast<int>( std::max( 1.0, 1.0 / settings->river_scale ) );
    int river_scale = static_cast<int>( std::max( 1.0, settings->river_scale ) );
    // West/North endpoints of rivers
    std::vector<point_om_omt> river_start;
    // East/South endpoints of rivers
    std::vector<point_om_omt> river_end;

    // Determine points where rivers & roads should connect w/ adjacent maps
    // optimized comparison.
    const oter_id river_center( "river_center" );

    if( north != nullptr ) {
        for( int i = 2; i < OMAPX - 2; i++ ) {
            const tripoint_om_omt p_neighbour( i, OMAPY - 1, 0 );
            const tripoint_om_omt p_mine( i, 0, 0 );

            if( is_river( north->ter( p_neighbour ) ) ) {
                ter_set( p_mine, river_center );
            }
            if( is_river( north->ter( p_neighbour ) ) &&
                is_river( north->ter( p_neighbour + point_east ) ) &&
                is_river( north->ter( p_neighbour + point_west ) ) ) {
                if( one_in( river_chance ) && ( river_start.empty() ||
                                                river_start[river_start.size() - 1].x() < ( i - 6 ) * river_scale ) ) {
                    river_start.push_back( p_mine.xy() );
                }
            }
        }
    }
    size_t rivers_from_north = river_start.size();
    if( west != nullptr ) {
        for( int i = 2; i < OMAPY - 2; i++ ) {
            const tripoint_om_omt p_neighbour( OMAPX - 1, i, 0 );
            const tripoint_om_omt p_mine( 0, i, 0 );

            if( is_river( west->ter( p_neighbour ) ) ) {
                ter_set( p_mine, river_center );
            }
            if( is_river( west->ter( p_neighbour ) ) &&
                is_river( west->ter( p_neighbour + point_north ) ) &&
                is_river( west->ter( p_neighbour + point_south ) ) ) {
                if( one_in( river_chance ) && ( river_start.size() == rivers_from_north ||
                                                river_start[river_start.size() - 1].y() < ( i - 6 ) * river_scale ) ) {
                    river_start.push_back( p_mine.xy() );
                }
            }
        }
    }
    if( south != nullptr ) {
        for( int i = 2; i < OMAPX - 2; i++ ) {
            const tripoint_om_omt p_neighbour( i, 0, 0 );
            const tripoint_om_omt p_mine( i, OMAPY - 1, 0 );

            if( is_river( south->ter( p_neighbour ) ) ) {
                ter_set( p_mine, river_center );
            }
            if( is_river( south->ter( p_neighbour ) ) &&
                is_river( south->ter( p_neighbour + point_east ) ) &&
                is_river( south->ter( p_neighbour + point_west ) ) ) {
                if( river_end.empty() ||
                    river_end[river_end.size() - 1].x() < i - 6 ) {
                    river_end.push_back( p_mine.xy() );
                }
            }
        }
    }
    size_t rivers_to_south = river_end.size();
    if( east != nullptr ) {
        for( int i = 2; i < OMAPY - 2; i++ ) {
            const tripoint_om_omt p_neighbour( 0, i, 0 );
            const tripoint_om_omt p_mine( OMAPX - 1, i, 0 );

            if( is_river( east->ter( p_neighbour ) ) ) {
                ter_set( p_mine, river_center );
            }
            if( is_river( east->ter( p_neighbour ) ) &&
                is_river( east->ter( p_neighbour + point_north ) ) &&
                is_river( east->ter( p_neighbour + point_south ) ) ) {
                if( river_end.size() == rivers_to_south ||
                    river_end[river_end.size() - 1].y() < i - 6 ) {
                    river_end.push_back( p_mine.xy() );
                }
            }
        }
    }

    // Even up the start and end points of rivers. (difference of 1 is acceptable)
    // Also ensure there's at least one of each.
    std::vector<point_om_omt> new_rivers;
    if( north == nullptr || west == nullptr ) {
        while( river_start.empty() || river_start.size() + 1 < river_end.size() ) {
            new_rivers.clear();
            if( north == nullptr && one_in( river_chance ) ) {
                new_rivers.emplace_back( rng( 10, OMAPX - 11 ), 0 );
            }
            if( west == nullptr && one_in( river_chance ) ) {
                new_rivers.emplace_back( 0, rng( 10, OMAPY - 11 ) );
            }
            river_start.push_back( random_entry( new_rivers ) );
        }
    }
    if( south == nullptr || east == nullptr ) {
        while( river_end.empty() || river_end.size() + 1 < river_start.size() ) {
            new_rivers.clear();
            if( south == nullptr && one_in( river_chance ) ) {
                new_rivers.emplace_back( rng( 10, OMAPX - 11 ), OMAPY - 1 );
            }
            if( east == nullptr && one_in( river_chance ) ) {
                new_rivers.emplace_back( OMAPX - 1, rng( 10, OMAPY - 11 ) );
            }
            river_end.push_back( random_entry( new_rivers ) );
        }
    }

    // Now actually place those rivers.
    if( river_start.size() > river_end.size() && !river_end.empty() ) {
        std::vector<point_om_omt> river_end_copy = river_end;
        while( !river_start.empty() ) {
            const point_om_omt start = random_entry_removed( river_start );
            if( !river_end.empty() ) {
                place_river( start, river_end[0] );
                river_end.erase( river_end.begin() );
            } else {
                place_river( start, random_entry( river_end_copy ) );
            }
        }
    } else if( river_end.size() > river_start.size() && !river_start.empty() ) {
        std::vector<point_om_omt> river_start_copy = river_start;
        while( !river_end.empty() ) {
            const point_om_omt end = random_entry_removed( river_end );
            if( !river_start.empty() ) {
                place_river( river_start[0], end );
                river_start.erase( river_start.begin() );
            } else {
                place_river( random_entry( river_start_copy ), end );
            }
        }
    } else if( !river_end.empty() ) {
        if( river_start.size() != river_end.size() ) {
            river_start.emplace_back( rng( OMAPX / 4, ( OMAPX * 3 ) / 4 ),
                                      rng( OMAPY / 4, ( OMAPY * 3 ) / 4 ) );
        }
        for( size_t i = 0; i < river_start.size(); i++ ) {
            place_river( river_start[i], river_end[i] );
        }
    }
}

void overmap::place_swamps()
{
    // Buffer our river terrains by a variable radius and increment a counter for the location each
    // time it's included in a buffer. It's a floodplain that we'll then intersect later with some
    // noise to adjust how frequently it occurs.
    std::vector<std::vector<int>> floodplain( OMAPX, std::vector<int>( OMAPY, 0 ) );
    for( int x = 0; x < OMAPX; x++ ) {
        for( int y = 0; y < OMAPY; y++ ) {
            const tripoint_om_omt pos( x, y, 0 );
            if( is_ot_match( "river", ter( pos ), ot_match_type::contains ) ) {
                std::vector<point_om_omt> buffered_points = closest_points_first( pos.xy(),
                        rng(
                            settings->overmap_forest.river_floodplain_buffer_distance_min,
                            settings->overmap_forest.river_floodplain_buffer_distance_max ) );
                for( const point_om_omt &p : buffered_points )  {
                    if( !inbounds( p ) ) {
                        continue;
                    }
                    floodplain[p.x()][p.y()] += 1;
                }
            }
        }
    }

    const oter_id forest_water( "forest_water" );

    // Get a layer of noise to use in conjunction with our river buffered floodplain.
    const om_noise::om_noise_layer_floodplain f( global_base_point(), g->get_seed() );

    for( int x = 0; x < OMAPX; x++ ) {
        for( int y = 0; y < OMAPY; y++ ) {
            const tripoint_om_omt pos( x, y, 0 );
            // If this location isn't a forest, there's nothing to do here. We'll only grow swamps in existing
            // forest terrain.
            if( !is_ot_match( "forest", ter( pos ), ot_match_type::contains ) ) {
                continue;
            }

            // If this was a part of our buffered floodplain, and the noise here meets the threshold, and the one_in rng
            // triggers, then we should flood this location and make it a swamp.
            const bool should_flood = ( floodplain[x][y] > 0 && !one_in( floodplain[x][y] ) && f.noise_at( { x, y } )
                                        > settings->overmap_forest.noise_threshold_swamp_adjacent_water );

            // If this location meets our isolated swamp threshold, regardless of floodplain values, we'll make it
            // into a swamp.
            const bool should_isolated_swamp = f.noise_at( pos.xy() ) >
                                               settings->overmap_forest.noise_threshold_swamp_isolated;
            if( should_flood || should_isolated_swamp )  {
                ter_set( pos, forest_water );
            }
        }
    }
}

void overmap::place_roads( const overmap *north, const overmap *east, const overmap *south,
                           const overmap *west )
{
    const overmap_connection_id local_road( "local_road" );
    std::vector<tripoint_om_omt> &roads_out = connections_out[local_road];

    // Ideally we should have at least two exit points for roads, on different sides
    if( roads_out.size() < 2 ) {
        std::vector<tripoint_om_omt> viable_roads;
        tripoint_om_omt tmp;
        // Populate viable_roads with one point for each neighborless side.
        // Make sure these points don't conflict with rivers.

        std::array < int, OMAPX - 20 > omap_num;
        for( int i = 0; i < 160; i++ ) {
            omap_num[i] = i + 10;
        }

        if( north == nullptr ) {
            std::shuffle( omap_num.begin(), omap_num.end(), rng_get_engine() );
            for( const auto &i : omap_num ) {
                tmp = tripoint_om_omt( i, 0, 0 );
                if( !( is_river( ter( tmp ) ) || is_river( ter( tmp + point_east ) ) ||
                       is_river( ter( tmp + point_west ) ) ) ) {
                    viable_roads.push_back( tmp );
                    break;
                }
            }
        }
        if( east == nullptr ) {
            std::shuffle( omap_num.begin(), omap_num.end(), rng_get_engine() );
            for( const auto &i : omap_num ) {
                tmp = tripoint_om_omt( OMAPX - 1, i, 0 );
                if( !( is_river( ter( tmp ) ) || is_river( ter( tmp + point_north ) ) ||
                       is_river( ter( tmp + point_south ) ) ) ) {
                    viable_roads.push_back( tmp );
                    break;
                }
            }
        }
        if( south == nullptr ) {
            std::shuffle( omap_num.begin(), omap_num.end(), rng_get_engine() );
            for( const auto &i : omap_num ) {
                tmp = tripoint_om_omt( i, OMAPY - 1, 0 );
                if( !( is_river( ter( tmp ) ) || is_river( ter( tmp + point_east ) ) ||
                       is_river( ter( tmp + point_west ) ) ) ) {
                    viable_roads.push_back( tmp );
                    break;
                }
            }
        }
        if( west == nullptr ) {
            std::shuffle( omap_num.begin(), omap_num.end(), rng_get_engine() );
            for( const auto &i : omap_num ) {
                tmp = tripoint_om_omt( 0, i, 0 );
                if( !( is_river( ter( tmp ) ) || is_river( ter( tmp + point_north ) ) ||
                       is_river( ter( tmp + point_south ) ) ) ) {
                    viable_roads.push_back( tmp );
                    break;
                }
            }
        }
        while( roads_out.size() < 2 && !viable_roads.empty() ) {
            roads_out.push_back( random_entry_removed( viable_roads ) );
        }
    }

    std::vector<point_om_omt> road_points; // cities and roads_out together
    // Compile our master list of roads; it's less messy if roads_out is first
    road_points.reserve( roads_out.size() + cities.size() );
    for( const auto &elem : roads_out ) {
        road_points.emplace_back( elem.xy() );
    }
    for( const auto &elem : cities ) {
        road_points.emplace_back( elem.pos );
    }

    // And finally connect them via roads.
    connect_closest_points( road_points, 0, *local_road );
}

void overmap::place_river( point_om_omt pa, point_om_omt pb )
{
    const oter_id river_center( "river_center" );
    int river_chance = static_cast<int>( std::max( 1.0, 1.0 / settings->river_scale ) );
    int river_scale = static_cast<int>( std::max( 1.0, settings->river_scale ) );
    point_om_omt p2( pa );
    do {
        p2.x() += rng( -1, 1 );
        p2.y() += rng( -1, 1 );
        if( p2.x() < 0 ) {
            p2.x() = 0;
        }
        if( p2.x() > OMAPX - 1 ) {
            p2.x() = OMAPX - 1;
        }
        if( p2.y() < 0 ) {
            p2.y() = 0;
        }
        if( p2.y() > OMAPY - 1 ) {
            p2.y() = OMAPY - 1;
        }
        for( int i = -1 * river_scale; i <= 1 * river_scale; i++ ) {
            for( int j = -1 * river_scale; j <= 1 * river_scale; j++ ) {
                tripoint_om_omt p( p2 + point( j, i ), 0 );
                if( p.y() >= 0 && p.y() < OMAPY && p.x() >= 0 && p.x() < OMAPX ) {
                    if( !ter( p )->is_lake() && one_in( river_chance ) ) {
                        ter_set( p, river_center );
                    }
                }
            }
        }
        if( pb.x() > p2.x() && ( rng( 0, static_cast<int>( OMAPX * 1.2 ) - 1 ) < pb.x() - p2.x() ||
                                 ( rng( 0, static_cast<int>( OMAPX * 0.2 ) - 1 ) > pb.x() - p2.x() &&
                                   rng( 0, static_cast<int>( OMAPY * 0.2 ) - 1 ) > std::abs( pb.y() - p2.y() ) ) ) ) {
            p2.x()++;
        }
        if( pb.x() < p2.x() && ( rng( 0, static_cast<int>( OMAPX * 1.2 ) - 1 ) < p2.x() - pb.x() ||
                                 ( rng( 0, static_cast<int>( OMAPX * 0.2 ) - 1 ) > p2.x() - pb.x() &&
                                   rng( 0, static_cast<int>( OMAPY * 0.2 ) - 1 ) > std::abs( pb.y() - p2.y() ) ) ) ) {
            p2.x()--;
        }
        if( pb.y() > p2.y() && ( rng( 0, static_cast<int>( OMAPY * 1.2 ) - 1 ) < pb.y() - p2.y() ||
                                 ( rng( 0, static_cast<int>( OMAPY * 0.2 ) - 1 ) > pb.y() - p2.y() &&
                                   rng( 0, static_cast<int>( OMAPX * 0.2 ) - 1 ) > std::abs( p2.x() - pb.x() ) ) ) ) {
            p2.y()++;
        }
        if( pb.y() < p2.y() && ( rng( 0, static_cast<int>( OMAPY * 1.2 ) - 1 ) < p2.y() - pb.y() ||
                                 ( rng( 0, static_cast<int>( OMAPY * 0.2 ) - 1 ) > p2.y() - pb.y() &&
                                   rng( 0, static_cast<int>( OMAPX * 0.2 ) - 1 ) > std::abs( p2.x() - pb.x() ) ) ) ) {
            p2.y()--;
        }
        p2.x() += rng( -1, 1 );
        p2.y() += rng( -1, 1 );
        if( p2.x() < 0 ) {
            p2.x() = 0;
        }
        if( p2.x() > OMAPX - 1 ) {
            p2.x() = OMAPX - 2;
        }
        if( p2.y() < 0 ) {
            p2.y() = 0;
        }
        if( p2.y() > OMAPY - 1 ) {
            p2.y() = OMAPY - 1;
        }
        for( int i = -1 * river_scale; i <= 1 * river_scale; i++ ) {
            for( int j = -1 * river_scale; j <= 1 * river_scale; j++ ) {
                // We don't want our riverbanks touching the edge of the map for many reasons
                tripoint_om_omt p( p2 + point( j, i ), 0 );
                if( inbounds( p, 1 ) ||
                    // UNLESS, of course, that's where the river is headed!
                    ( std::abs( pb.y() - p.y() ) < 4 && std::abs( pb.x() - p.x() ) < 4 ) ) {
                    if( !inbounds( p ) ) {
                        continue;
                    }

                    if( !ter( p )->is_lake() && one_in( river_chance ) ) {
                        ter_set( p, river_center );
                    }
                }
            }
        }
    } while( pb != p2 );
}

void overmap::place_cities()
{
    int op_city_size = get_option<int>( "CITY_SIZE" );
    if( op_city_size <= 0 ) {
        return;
    }
    int op_city_spacing = get_option<int>( "CITY_SPACING" );
    const city_settings &city_spec = settings->city_spec;
    // spacing dictates how much of the map is covered in cities
    //   city  |  cities  |   size N cities per overmap
    // spacing | % of map |  2  |  4  |  8  |  12 |  16
    //     0   |   ~99    |2025 | 506 | 126 |  56 |  31
    //     1   |    50    |1012 | 253 |  63 |  28 |  15
    //     2   |    25    | 506 | 126 |  31 |  14 |   7
    //     3   |    12    | 253 |  63 |  15 |   7 |   3
    //     4   |     6    | 126 |  31 |   7 |   3 |   1
    //     5   |     3    |  63 |  15 |   3 |   1 |   0
    //     6   |     1    |  31 |   7 |   1 |   0 |   0
    //     7   |     0    |  15 |   3 |   0 |   0 |   0
    //     8   |     0    |   7 |   1 |   0 |   0 |   0

    const double omts_per_overmap = OMAPX * OMAPY;
    const double city_map_coverage_ratio = 1.0 / std::pow( 2.0, op_city_spacing );
    const double omts_per_city = ( op_city_size * 2 + 1 ) * ( op_city_size * 2 + 1 ) * 3 / 4.0;

    // how many cities on this overmap?
    const int NUM_CITIES =
        roll_remainder( omts_per_overmap * city_map_coverage_ratio / omts_per_city );

    const overmap_connection_id sewer_tunnel( "sewer_tunnel" );
    const overmap_connection_id local_road_id( "local_road" );
    const overmap_connection &local_road( *local_road_id );

    // if there is only a single free tile, the probability of NOT finding it after MAX_PLACEMENT_ATTEMTPS attempts
    // is (1 - 1/(OMAPX * OMAPY))^MAX_PLACEMENT_ATTEMTPS = approx. 36% for the OMAPX=OMAPY=180 and MAX_PLACEMENT_ATTEMTPS=OMAPX * OMAPY
    const int MAX_PLACEMENT_ATTEMTPS = OMAPX * OMAPY;
    int placement_attempts = 0;
    int finale_distance = 1;
    std::unique_ptr<std::array<map_layer, OVERMAP_LAYERS>> layer_backup_p(
                new std::array<map_layer, OVERMAP_LAYERS>() );
    std::array<map_layer, 21UL> &layer_backup = *layer_backup_p;

    // place a seed for NUM_CITIES cities, and maybe one more
    while( cities.size() < static_cast<size_t>( NUM_CITIES ) &&
           placement_attempts < MAX_PLACEMENT_ATTEMTPS ) {
        placement_attempts++;
        city tmp;
        // randomly make some cities smaller or larger
        // guarantee placement of a finale/vault tile in large/huge cities
        int size = rng( op_city_size - 1, op_city_size + 1 );
        if( one_in( 3 ) ) {      // 33% tiny
            tmp.attempt_finale = false;
            size = 1;
        } else if( one_in( 2 ) ) { // 33% small
            tmp.attempt_finale = false;
            size = size * 2 / 3;
        } else if( one_in( 2 ) ) { // 17% large
            tmp.attempt_finale = true;
            size = size * 3 / 2;
            finale_distance = 5;
        } else {                 // 17% huge
            tmp.attempt_finale = true;
            size = size * 2;
            finale_distance = 15;
        }
        //also avoid a finale if the city spec has none
        if( !city_spec.finales.finalized ) {
            tmp.attempt_finale = false;
        }
        size = std::max( size, 1 );

        // TODO: put cities closer to the edge when they can span overmaps
        // don't draw cities across the edge of the map, they will get clipped
        const tripoint_om_omt p{ rng( size - 1, OMAPX - size ), rng( size - 1, OMAPY - size ), 0 };
        //make a backup of the map
        layer_backup = layer;
        tmp.finale_placed = false;
        int finale_attempts = 0;
        int finale_max_tries = 1500;
        //attempt to generate a city with a finale if it's not tiny. If it's tiny just run once via a do while.
        do  {
            //std::unordered_map<tripoint_om_omt, std::string> oter_id_migrations;
            if( ter( p ) == settings->default_oter ) {
                placement_attempts = 0;
                ter_set( p, oter_id( "road_nesw_manhole" ) ); // every city starts with an intersection
                ter_set( p + tripoint_below, oter_id( "sewer_isolated" ) );
                tmp.pos = p.xy();
                tmp.size = size;
                ;
                tmp.finale_counter = rng( 0, finale_distance );
                cities.push_back( tmp );

                const auto start_dir = om_direction::random();
                auto cur_dir = start_dir;
                std::vector<tripoint_om_omt> sewers;

                do {
                    build_city_street( local_road, tmp.pos, size, cur_dir, tmp, sewers );
                } while( ( cur_dir = om_direction::turn_right( cur_dir ) ) != start_dir );
                for( const tripoint_om_omt &p : sewers ) {
                    build_connection( tmp.pos, p.xy(), p.z(), *sewer_tunnel, false );
                }

                //if the city finale failed to place, restore from last backup and try again at the top of the loop
                if( !tmp.finale_placed  && tmp.attempt_finale && finale_attempts < finale_max_tries ) {
                    layer = layer_backup;
                }
            }
            finale_attempts++;
        } while( ( tmp.attempt_finale && !tmp.finale_placed &&
                   finale_attempts < finale_max_tries ) );
    }
}

overmap_special_id overmap::pick_random_building_to_place( int town_dist,
        bool attempt_finale_place ) const
{
    const city_settings &city_spec = settings->city_spec;
    int shop_radius = city_spec.shop_radius;
    int park_radius = city_spec.park_radius;

    int shop_sigma = city_spec.shop_sigma;
    int park_sigma = city_spec.park_sigma;

    //Normally distribute shops and parks
    //Clamp at 1/2 radius to prevent houses from spawning in the city center.
    //Parks are nearly guaranteed to have a non-zero chance of spawning anywhere in the city.
    int shop_normal = std::max( static_cast<int>( normal_roll( shop_radius, shop_sigma ) ),
                                shop_radius );
    int park_normal = std::max( static_cast<int>( normal_roll( park_radius, park_sigma ) ),
                                park_radius );

    if( attempt_finale_place ) {
        //return overmap_special_id("Military Outpost");
        //return overmap_special_id( "megastore" );
        return city_spec.pick_finale();
    } else if( shop_normal > town_dist ) {
        return city_spec.pick_shop();
    } else if( park_normal > town_dist ) {
        return city_spec.pick_park();
    } else {
        return city_spec.pick_house();
    }
}

bool overmap::place_building( const tripoint_om_omt &p, om_direction::type dir, city &town,
                              bool attempt_finale_place )
{
    const tripoint_om_omt building_pos = p + om_direction::displace( dir );
    const om_direction::type building_dir = om_direction::opposite( dir );

    const int town_dist = ( trig_dist( building_pos.xy(), town.pos ) * 100 ) / std::max( town.size, 1 );

    for( size_t retries = 10; retries > 0; --retries ) {
        const overmap_special_id building_tid = pick_random_building_to_place( town_dist,
                                                attempt_finale_place );

        if( can_place_special( *building_tid, building_pos, building_dir, false ) ) {
            place_special( *building_tid, building_pos, building_dir, town, false, false );
            return( true );
            break;
        }
    }
    return( false );
}
//city passed by reference as it's modified by the function, which calls itself recursively
void overmap::build_city_street(
    const overmap_connection &connection, const point_om_omt &p, int cs,
    om_direction::type dir, city &town, std::vector<tripoint_om_omt> &sewers,
    int block_width )
{
    int c = cs;
    int croad = cs;

    if( dir == om_direction::type::invalid ) {
        debugmsg( "Invalid road direction." );
        return;
    }

    const pf::directed_path<point_om_omt> street_path = lay_out_street( connection, p, dir, cs + 1 );

    if( street_path.nodes.size() <= 1 ) {
        return; // Don't bother.
    }
    // Build the actual street.
    build_connection( connection, street_path, 0 );
    // Grow in the stated direction, sprouting off sub-roads and placing buildings as we go.
    const auto from = std::next( street_path.nodes.begin() );
    const auto to = street_path.nodes.end();

    //Alternate wide and thin blocks
    int new_width = block_width == 2 ? rng( 3, 5 ) : 2;

    for( auto iter = from; iter != to; ++iter ) {
        --c;

        const tripoint_om_omt rp( iter->pos, 0 );
        if( c >= 2 && c < croad - block_width ) {
            croad = c;
            int left = cs - rng( 1, 3 );
            int right = cs - rng( 1, 3 );

            //Remove 1 length road nubs
            if( left == 1 ) {
                left++;
            }
            if( right == 1 ) {
                right++;
            }

            build_city_street( connection, iter->pos, left, om_direction::turn_left( dir ),
                               town, sewers, new_width );

            build_city_street( connection, iter->pos, right, om_direction::turn_right( dir ),
                               town, sewers, new_width );

            const oter_id &oter = ter( rp );
            // TODO: Get rid of the hardcoded terrain ids.
            if( one_in( 8 ) && oter->get_line() == 15 && oter->type_is( oter_type_id( "road" ) ) ) {
                ter_set( rp, oter_id( "road_nesw_manhole" ) );
                ter_set( rp + tripoint_below, oter_id( "sewer_isolated" ) );
                sewers.push_back( rp + tripoint_below );
            }
        }
        bool attempt_finale_place = false;
        // place a finale somewhere within the first 15 buildings
        if( town.finale_counter == 0 && !town.finale_placed && town.attempt_finale ) {
            attempt_finale_place = true;
        } else {
            town.finale_counter--;
        }
        if( !one_in( BUILDINGCHANCE ) ) {
            if( attempt_finale_place && !town.finale_placed ) {
                //attempt to place a finale, confirm that it was placed.
                if( place_building( rp, om_direction::turn_left( dir ), town, true ) ) {
                    town.finale_placed = true;
                } else { // if the finale fails to place stop trying. This prevents the finale getting placed at the edge of town.
                    attempt_finale_place = false;
                    town.finale_counter = -1;
                }
            } else {
                place_building( rp, om_direction::turn_left( dir ), town, false );
            }
        }
        if( !one_in( BUILDINGCHANCE ) ) {

            if( attempt_finale_place && !town.finale_placed ) {
                //attempt to place a finale, confirm that it was placed.
                if( place_building( rp, om_direction::turn_left( dir ), town, true ) ) {
                    town.finale_placed = true;
                } else { // if the finale fails to place stop trying. This prevents the finale getting placed at the edge of town.
                    attempt_finale_place = false;
                    town.finale_counter = -1;
                }
            } else {
                place_building( rp, om_direction::turn_right( dir ), town, false );
            }
        }
    }

    // If we're big, make a right turn at the edge of town.
    // Seems to make little neighborhoods.
    cs -= rng( 1, 3 );

    if( cs >= 2 && c == 0 ) {
        const auto &last_node = street_path.nodes.back();
        const auto rnd_dir = om_direction::turn_random( dir );
        build_city_street( connection, last_node.pos, cs, rnd_dir, town, sewers );
        if( one_in( 5 ) ) {
            build_city_street( connection, last_node.pos, cs, om_direction::opposite( rnd_dir ),
                               town, sewers, new_width );
        }
    }
}

pf::directed_path<point_om_omt> overmap::lay_out_connection(
    const overmap_connection &connection, const point_om_omt &source, const point_om_omt &dest,
    int z, const bool must_be_unexplored ) const
{
    const pf::two_node_scoring_fn<point_om_omt> estimate =
    [&]( pf::directed_node<point_om_omt> cur, std::optional<pf::directed_node<point_om_omt>> prev ) {
        const auto &id( ter( tripoint_om_omt( cur.pos, z ) ) );

        const overmap_connection::subtype *subtype = connection.pick_subtype_for( id );

        if( !subtype ) {
            return pf::node_score::rejected;  // No option for this terrain.
        }

        const bool existing_connection = connection.has( id );

        // Only do this check if it needs to be unexplored and there isn't already a connection.
        if( must_be_unexplored && !existing_connection ) {
            // If this must be unexplored, check if we've already got a submap generated.
            const bool existing_submap = is_omt_generated( tripoint_om_omt( cur.pos, z ) );

            // If there is an existing submap, this area has already been explored and this
            // isn't a valid placement.
            if( existing_submap ) {
                return pf::node_score::rejected;
            }
        }

        if( existing_connection && id->is_rotatable() && cur.dir != om_direction::type::invalid &&
            !om_direction::are_parallel( id->get_dir(), cur.dir ) ) {
            return pf::node_score::rejected; // Can't intersect.
        }

        if( prev && prev->dir != om_direction::type::invalid && prev->dir != cur.dir ) {
            // Direction has changed.
            const oter_id &prev_id = ter( tripoint_om_omt( prev->pos, z ) );
            const overmap_connection::subtype *prev_subtype = connection.pick_subtype_for( prev_id );

            if( !prev_subtype || !prev_subtype->allows_turns() ) {
                return pf::node_score::rejected;
            }
        }

        if( prev && prev->dir == om_direction::type::invalid && cur.dir != om_direction::type::invalid ) {
            // Non-linear connections starting near overmap border should always be perpendicular to that border
            const oter_id &prev_id = ter( tripoint_om_omt( prev->pos, z ) );

            if( !connection.can_start_at( prev_id ) && !inbounds( cur.pos, 1 ) &&
                inbounds( prev->pos + om_direction::displace( om_direction::opposite( cur.dir ), 1 ) ) ) {
                return pf::node_score::rejected;
            }
        }

        int score = subtype->is_orthogonal() ?
                    manhattan_dist( dest, cur.pos ) :
                    trig_dist( dest, cur.pos );
        if( !existing_connection ) {
            // Prefer existing connections
            score *= 5;
        }
        if( !inbounds( cur.pos, 1 ) ) {
            // Roads running next to overmap edge often looks unnatural and weird, better avoid them
            score *= 2;
        }

        return pf::node_score( subtype->basic_cost, score );
    };

    return pf::greedy_path( source, dest, point_om_omt( OMAPX, OMAPY ), estimate );
}

static pf::directed_path<point_om_omt> straight_path( const point_om_omt &source,
        om_direction::type dir, size_t len )
{
    pf::directed_path<point_om_omt> res;
    if( len == 0 ) {
        return res;
    }
    point_om_omt p = source;
    res.nodes.reserve( len );
    for( size_t i = 0; i + 1 < len; ++i ) {
        res.nodes.emplace_back( p, dir );
        p += om_direction::displace( dir );
    }
    res.nodes.emplace_back( p, om_direction::type::invalid );
    return res;
}

pf::directed_path<point_om_omt> overmap::lay_out_street( const overmap_connection &connection,
        const point_om_omt &source, om_direction::type dir, size_t len ) const
{
    const tripoint_om_omt from( source, 0 );
    // See if we need to make another one "step" further.
    const tripoint_om_omt en_pos = from + om_direction::displace( dir, len + 1 );
    if( inbounds( en_pos, 1 ) && connection.has( ter( en_pos ) ) ) {
        ++len;
    }

    size_t actual_len = 0;

    while( actual_len < len ) {
        const tripoint_om_omt pos = from + om_direction::displace( dir, actual_len );

        if( !inbounds( pos, 1 ) ) {
            break;  // Don't approach overmap bounds.
        }

        const oter_id &ter_id = ter( pos );

        if( ter_id->is_river() || !connection.pick_subtype_for( ter_id ) ) {
            break;
        }

        bool collided = false;
        int collisions = 0;
        for( int i = -1; i <= 1; i++ ) {
            if( collided ) {
                break;
            }
            for( int j = -1; j <= 1; j++ ) {
                const tripoint_om_omt checkp = pos + tripoint( i, j, 0 );

                if( checkp != pos + om_direction::displace( dir, 1 ) &&
                    checkp != pos + om_direction::displace( om_direction::opposite( dir ), 1 ) &&
                    checkp != pos ) {
                    if( is_ot_match( "road", ter( checkp ), ot_match_type::type ) ) {
                        collisions++;
                    }
                }
            }

            //Stop roads from running right next to eachother
            if( collisions >= 3 ) {
                collided = true;
                break;
            }
        }
        if( collided ) {
            break;
        }

        ++actual_len;

        if( actual_len > 1 && connection.has( ter_id ) ) {
            break;  // Stop here.
        }
    }

    return straight_path( source, dir, actual_len );
}

const std::vector<point_om_omt> &overmap_connection_cache::get_all( const overmap_connection_id &id,
        const int z )
{
    auto outer = cache.find( id );
    if( outer != cache.end() ) {
        auto inner = outer->second.find( z );
        if( inner != outer->second.end() ) {
            return inner->second;
        }
    }
    // Return an empty vector if the keys do not exist
    static const std::vector<point_om_omt> empty;
    return empty;
}

std::vector<point_om_omt> overmap_connection_cache::get_closests(
    const overmap_connection_id &id, const int z, const point_om_omt &pos )
{
    std::vector<point_om_omt> sorted = get_all( id, z );
    std::sort( sorted.begin(), sorted.end(), [&pos]( point_om_omt & a, point_om_omt & b ) {
        return rl_dist( pos, a ) < rl_dist( pos, b );
    } );

    return sorted;
}

void overmap_connection_cache::add( const overmap_connection_id &id, const int z,
                                    const point_om_omt &pos )
{
    auto outer = cache.find( id );
    if( outer == cache.end() ) {
        outer = cache.emplace( id, std::map<int, std::vector<point_om_omt>> {} ).first;
    }
    auto inner = outer->second.find( z );
    if( inner == outer->second.end() ) {
        inner = outer->second.emplace( z, std::vector<point_om_omt> {} ).first;
    }
    inner->second.push_back( pos );
}

bool overmap::build_connection(
    const overmap_connection &connection, const pf::directed_path<point_om_omt> &path, int z,
    const cube_direction initial_dir )
{
    if( path.nodes.empty() ) {
        return false;
    }

    om_direction::type prev_dir =
        om_direction::from_cube( initial_dir, "Up and down connections not yet supported" );

    const pf::directed_node<point_om_omt> start = path.nodes.front();
    const pf::directed_node<point_om_omt> end = path.nodes.back();

    // Clear the cache before laying a road so that roads are consistent and new road types
    // are randomly chosen per road, not per load / game
    connection.clear_subtype_cache();

    for( const auto &node : path.nodes ) {
        const tripoint_om_omt pos( node.pos, z );
        const oter_id &ter_id = ter( pos );
        const om_direction::type new_dir = node.dir;
        const overmap_connection::subtype *subtype = connection.pick_subtype_for( ter_id );

        if( !subtype ) {
            debugmsg( "No suitable subtype of connection \"%s\" found for \"%s\".", connection.id.c_str(),
                      ter_id.id().c_str() );
            return false;
        }

        if( subtype->terrain->is_linear() ) {
            size_t new_line = connection.has( ter_id ) ? ter_id->get_line() : 0;

            if( new_dir != om_direction::type::invalid ) {
                new_line = om_lines::set_segment( new_line, new_dir );
            }

            if( prev_dir != om_direction::type::invalid ) {
                new_line = om_lines::set_segment( new_line, om_direction::opposite( prev_dir ) );
            }

            for( const om_direction::type dir : om_direction::all ) {
                const tripoint_om_omt np( pos + om_direction::displace( dir ) );

                if( inbounds( np ) ) {
                    const oter_id &near_id = ter( np );

                    if( connection.has( near_id ) ) {
                        if( near_id->is_linear() ) {
                            const size_t near_line = near_id->get_line();

                            if( om_lines::is_straight( near_line ) || om_lines::has_segment( near_line, new_dir ) ) {
                                // Mutual connection.
                                const size_t new_near_line = om_lines::set_segment( near_line, om_direction::opposite( dir ) );
                                ter_set( np, near_id->get_type_id()->get_linear( new_near_line ) );
                                new_line = om_lines::set_segment( new_line, dir );
                            }
                        } else if( near_id->is_rotatable() && om_direction::are_parallel( dir, near_id->get_dir() ) ) {
                            new_line = om_lines::set_segment( new_line, dir );
                        }
                    }
                } else if( pos.xy() == start.pos || pos.xy() == end.pos ) {
                    // Only automatically connect to out of bounds locations if we're the start or end of this path.
                    new_line = om_lines::set_segment( new_line, dir );

                    // Add this connection point to our connections out.
                    std::vector<tripoint_om_omt> &outs = connections_out[connection.id];
                    const auto existing_out = std::find_if( outs.begin(),
                    outs.end(), [pos]( const tripoint_om_omt & c ) {
                        return c == pos;
                    } );
                    if( existing_out == outs.end() ) {
                        outs.emplace_back( pos );
                    }
                }
            }

            if( new_line == om_lines::invalid ) {
                debugmsg( "Invalid path for connection \"%s\".", connection.id.c_str() );
                return false;
            }

            ter_set( pos, subtype->terrain->get_linear( new_line ) );
        } else if( new_dir != om_direction::type::invalid ) {
            ter_set( pos, subtype->terrain->get_rotated( new_dir ) );
        } else if( prev_dir != om_direction::type::invalid ) {
            ter_set( pos, subtype->terrain->get_rotated( prev_dir ) );
        }

        prev_dir = new_dir;
    }


    if( connection_cache ) {
        connection_cache->add( connection.id, z, start.pos );
    }
    // Elevate bridges here so each bridge get's it's own type
    // That way all bridges in an overmap (or more) are not just of one type
    if( z == 0 && connection.id.str() == "local_road" ) {
        std::vector<point_om_omt> bridge_points;
        std::string name;
        bool has_bridge = false;
        for( const auto &node : path.nodes ) {
            const tripoint_om_omt pos( node.pos, z );
            if( ter( pos )->has_flag( oter_flags::is_bridge ) ) {
                name = ter( pos )->get_type_id().str();
                bridge_points.emplace_back( pos.xy() );
                has_bridge = true;
            }
        }
        if( !has_bridge ) {
            return true;
        }
        if( oter_str_id( name + "_center_under_north" ).is_valid() ) {
            elevate_bridges(
                *this, bridge_points,
                name + "_road", name + "_under", name + "head_ground",
                name + "head_ramp", name + "_center_under", "road_ew", "road_ns"
            );
        } else {
            elevate_bridges(
                *this, bridge_points,
                name + "_road", name + "_under", name + "head_ground",
                name + "head_ramp", "", "road_ew", "road_ns"
            );
        }
    }
    return true;
}

bool overmap::build_connection( const point_om_omt &source, const point_om_omt &dest, int z,
                                const overmap_connection &connection, const bool must_be_unexplored,
                                const cube_direction initial_dir )
{
    return build_connection(
               connection, lay_out_connection( connection, source, dest, z, must_be_unexplored ),
               z, initial_dir );
}

void overmap::connect_closest_points( const std::vector<point_om_omt> &points, int z,
                                      const overmap_connection &connection )
{
    if( points.size() == 1 ) {
        return;
    }
    for( size_t i = 0; i < points.size(); ++i ) {
        int closest = -1;
        int k = 0;
        for( size_t j = i + 1; j < points.size(); j++ ) {
            const int distance = trig_dist( points[i], points[j] );
            if( distance < closest || closest < 0 ) {
                closest = distance;
                k = j;
            }
        }
        if( closest > 0 ) {
            // Use a pointer so it definitely will keep changes
            build_connection( points[i], points[k], z, connection, false );
        }
    }
}

bool overmap::check_ot( const std::string &otype, ot_match_type match_type,
                        const tripoint_om_omt &p ) const
{
    /// TODO: this check should be done by the caller. Probably.
    if( !inbounds( p ) ) {
        return false;
    }
    const oter_id &oter = ter( p );
    return is_ot_match( otype, oter, match_type );
}

bool overmap::check_overmap_special_type( const overmap_special_id &id,
        const tripoint_om_omt &location ) const
{
    // Try and find the special associated with this location.
    auto found_id = overmap_special_placements.find( location );

    // There was no special here, so bail.
    if( found_id == overmap_special_placements.end() ) {
        return false;
    }

    // Return whether the found special was a match with our requested id.
    return found_id->second == id;
}

std::optional<overmap_special_id> overmap::overmap_special_at( const tripoint_om_omt &p ) const
{
    auto it = overmap_special_placements.find( p );

    if( it == overmap_special_placements.end() ) {
        return std::nullopt;
    }

    return it->second;
}

void overmap::polish_rivers( const overmap *north, const overmap *east, const overmap *south,
                             const overmap *west )
{
    for( int x = 0; x < OMAPX; x++ ) {
        for( int y = 0; y < OMAPY; y++ ) {
            const tripoint_om_omt p = { x, y, 0 };

            if( !is_ot_match( "river", ter( p ), ot_match_type::prefix ) ) {
                continue;
            }

            // For ungenerated overmaps we're assuming that terra incognita is covered by water, and leaving polishing to its mapgen
            bool water_north = y != 0 ? is_river_or_lake( ter( { x, y - 1, 0} ) ) :
                               north != nullptr ? is_river_or_lake( north->ter( { x, OMAPY - 1, 0 } ) ) : true;
            bool water_west = x != 0 ? is_river_or_lake( ter( { x - 1, y, 0} ) ) :
                              west != nullptr ? is_river_or_lake( west->ter( { OMAPX - 1, y, 0 } ) ) : true;
            bool water_south = y != OMAPY - 1 ? is_river_or_lake( ter( { x, y + 1, 0} ) ) :
                               south != nullptr ? is_river_or_lake( south->ter( { x, 0, 0 } ) ) : true;
            bool water_east = x != OMAPX - 1 ? is_river_or_lake( ter( { x + 1, y, 0} ) ) :
                              east != nullptr ? is_river_or_lake( east->ter( { 0, y, 0 } ) ) : true;

            if( water_west ) {
                if( water_north ) {
                    if( water_south ) {
                        if( water_east ) {
                            // River on N, S, E, W;
                            // but we might need to take a "bite" out of the corner
                            if( ( x > 0 && y > 0 &&
                                  !is_river_or_lake( ter( { x - 1, y - 1, 0} ) ) ) ||
                                ( x > 0 && y == 0 && north != nullptr &&
                                  !is_river_or_lake( north->ter( { x - 1, OMAPY - 1, 0} ) ) ) ||
                                ( x == 0 && y > 0 && west != nullptr &&
                                  !is_river_or_lake( west->ter( { OMAPX - 1, y - 1, 0} ) ) )
                              ) {
                                ter_set( p, oter_id( "river_c_not_nw" ) );
                            } else if( ( x < OMAPX - 1 && y > 0 &&
                                         !is_river_or_lake( ter( { x + 1, y - 1, 0} ) ) ) ||
                                       ( x < OMAPX - 1 && y == 0 && north != nullptr &&
                                         !is_river_or_lake( north->ter( { x + 1, OMAPY - 1, 0 } ) ) ) ||
                                       ( x == OMAPX - 1 && y > 0 && east != nullptr &&
                                         !is_river_or_lake( east->ter( { 0, y - 1, 0 } ) ) )
                                     ) {
                                ter_set( p, oter_id( "river_c_not_ne" ) );
                            } else if( ( x > 0 && y < OMAPY - 1 &&
                                         !is_river_or_lake( ter( { x - 1, y + 1, 0} ) ) ) ||
                                       ( x > 0 && y == OMAPY - 1 && south != nullptr &&
                                         !is_river_or_lake( south->ter( { x - 1, 0, 0} ) ) ) ||
                                       ( x == 0 && y < OMAPY - 1 && west != nullptr &&
                                         !is_river_or_lake( west->ter( { OMAPX - 1, y + 1, 0} ) ) )
                                     ) {
                                ter_set( p, oter_id( "river_c_not_sw" ) );
                            } else if( ( x < OMAPX - 1 && y < OMAPY - 1 &&
                                         !is_river_or_lake( ter( { x + 1, y + 1, 0} ) ) ) ||
                                       ( x < OMAPX - 1 && y == OMAPY - 1 && south != nullptr &&
                                         !is_river_or_lake( south->ter( { x + 1, 0, 0 } ) ) ) ||
                                       ( x == OMAPX - 1 && y < OMAPY - 1 && east != nullptr &&
                                         !is_river_or_lake( east->ter( { 0, y + 1, 0 } ) ) )
                                     ) {
                                ter_set( p, oter_id( "river_c_not_se" ) );
                            } else {
                                ter_set( p, oter_id( "river_center" ) );
                            }
                        } else {
                            ter_set( p, oter_id( "river_east" ) );
                        }
                    } else {
                        if( water_east ) {
                            ter_set( p, oter_id( "river_south" ) );
                        } else {
                            ter_set( p, oter_id( "river_se" ) );
                        }
                    }
                } else {
                    if( water_south ) {
                        if( water_east ) {
                            ter_set( p, oter_id( "river_north" ) );
                        } else {
                            ter_set( p, oter_id( "river_ne" ) );
                        }
                    } else { // Means it's swampy
                        ter_set( p, oter_id( "forest_water" ) );
                    }
                }
            } else {
                if( water_north ) {
                    if( water_south ) {
                        if( water_east ) {
                            ter_set( p, oter_id( "river_west" ) );
                        } else { // Should never happen
                            ter_set( p, oter_id( "forest_water" ) );
                        }
                    } else {
                        if( water_east ) {
                            ter_set( p, oter_id( "river_sw" ) );
                        } else { // Should never happen
                            ter_set( p, oter_id( "forest_water" ) );
                        }
                    }
                } else {
                    if( water_south ) {
                        if( water_east ) {
                            ter_set( p, oter_id( "river_nw" ) );
                        } else { // Should never happen
                            ter_set( p, oter_id( "forest_water" ) );
                        }
                    } else { // Should never happen
                        ter_set( p, oter_id( "forest_water" ) );
                    }
                }
            }
        }
    }
}

std::string om_direction::name( type dir )
{
    static const std::array < std::string, size + 1 > names = {{
            translate_marker( "invalid" ), translate_marker( "north" ),
            translate_marker( "east" ), translate_marker( "south" ),
            translate_marker( "west" )
        }
    };
    return _( names[static_cast<size_t>( dir ) + 1] );
}

// new x = (x-c.x)*cos() - (y-c.y)*sin() + c.x
// new y = (x-c.x)*sin() + (y-c.y)*cos() + c.y
// r1x = 0*x - 1*y = -1*y, r1y = 1*x + y*0 = x
// r2x = -1*x - 0*y = -1*x , r2y = x*0 + y*-1 = -1*y
// r3x = x*0 - (-1*y) = y, r3y = x*-1 + y*0 = -1*x
// c=0,0, rot90 = (-y, x); rot180 = (-x, y); rot270 = (y, -x)
/*
    (0,0)(1,0)(2,0) 90 (0,0)(0,1)(0,2)       (-2,0)(-1,0)(0,0)
    (0,1)(1,1)(2,1) -> (-1,0)(-1,1)(-1,2) -> (-2,1)(-1,1)(0,1)
    (0,2)(1,2)(2,2)    (-2,0)(-2,1)(-2,2)    (-2,2)(-1,2)(0,2)
*/

point om_direction::rotate( point p, type dir )
{
    switch( dir ) {
        case om_direction::type::invalid:
        case om_direction::type::last:
            debugmsg( "Invalid overmap rotation (%d).", static_cast<int>( dir ) );
        // Intentional fallthrough.
        case om_direction::type::north:
            break;  // No need to do anything.
        case om_direction::type::east:
            return point( -p.y, p.x );
        case om_direction::type::south:
            return point( -p.x, -p.y );
        case om_direction::type::west:
            return point( p.y, -p.x );
    }
    return p;
}

tripoint om_direction::rotate( const tripoint &p, type dir )
{
    return tripoint( rotate( { p.xy() }, dir ), p.z );
}

uint32_t om_direction::rotate_symbol( uint32_t sym, type dir )
{
    return rotatable_symbols::get( sym, static_cast<int>( dir ) );
}

point om_direction::displace( type dir, int dist )
{
    return rotate( { 0, -dist }, dir );
}

inline om_direction::type rotate_internal( om_direction::type dir, int step )
{
    if( dir == om_direction::type::invalid ) {
        debugmsg( "Can't rotate an invalid overmap rotation." );
        return dir;
    }
    step = step % om_direction::size;
    return static_cast<om_direction::type>( ( static_cast<int>( dir ) + step ) % om_direction::size );
}

om_direction::type om_direction::add( type dir1, type dir2 )
{
    return rotate_internal( dir1, static_cast<int>( dir2 ) );
}

om_direction::type om_direction::turn_left( type dir )
{
    return rotate_internal( dir, -static_cast<int>( size ) / 4 );
}

om_direction::type om_direction::turn_right( type dir )
{
    return rotate_internal( dir, static_cast<int>( size ) / 4 );
}

om_direction::type om_direction::turn_random( type dir )
{
    return rng( 0, 1 ) ? turn_left( dir ) : turn_right( dir );
}

om_direction::type om_direction::opposite( type dir )
{
    return rotate_internal( dir, static_cast<int>( size ) / 2 );
}

om_direction::type om_direction::random()
{
    return static_cast<type>( rng( 0, size - 1 ) );
}

bool om_direction::are_parallel( type dir1, type dir2 )
{
    return dir1 == dir2 || dir1 == opposite( dir2 );
}

om_direction::type om_direction::from_cube( cube_direction c, const std::string &error_msg )
{
    switch( c ) {
        case cube_direction::north:
            return om_direction::type::north;
        case cube_direction::east:
            return om_direction::type::east;
        case cube_direction::south:
            return om_direction::type::south;
        case cube_direction::west:
            return om_direction::type::west;
        case cube_direction::above:
        case cube_direction::below:
            debugmsg( error_msg );
        // fallthrough
        case cube_direction::last:
            return om_direction::type::invalid;
    }
    debugmsg( "Invalid cube_direction" );
    return om_direction::type::invalid;
}

om_direction::type overmap::random_special_rotation( const overmap_special &special,
        const tripoint_om_omt &p, const bool must_be_unexplored ) const
{
    std::vector<om_direction::type> rotations( om_direction::size );
    const auto first = rotations.begin();
    auto last = first;

    int top_score = 0; // Maximal number of existing connections (roads).
    // Try to find the most suitable rotation: satisfy as many connections as possible with the existing terrain.
    for( om_direction::type r : om_direction::all ) {
        int score = 0; // Number of existing connections when rotated by 'r'.
        bool valid = true;

        for( const auto &con : special.connections ) {
            const tripoint_om_omt rp = p + om_direction::rotate( con.p, r );
            if( !inbounds( rp ) ) {
                valid = false;
                break;
            }
            const oter_id &oter = ter( rp );

            if( belongs_to_connection( con.connection, oter ) ) {
                ++score; // Found another one satisfied connection.
            } else if( !oter || con.existing || !con.connection->can_start_at( oter ) ) {
                valid = false;
                break;
            }
        }

        if( valid && score >= top_score ) {
            if( score > top_score ) {
                top_score = score;
                last = first; // New top score. Forget previous rotations.
            }
            *last = r;
            ++last;
        }

        if( !special.is_rotatable() ) {
            break;
        }
    }

    // Pick first valid rotation at random.
    std::shuffle( first, last, rng_get_engine() );
    const auto rotation = std::find_if( first, last, [&]( om_direction::type elem ) {
        return can_place_special( special, p, elem, must_be_unexplored );
    } );

    return rotation != last ? *rotation : om_direction::type::invalid;
}

bool overmap::can_place_special( const overmap_special &special, const tripoint_om_omt &p,
                                 om_direction::type dir, const bool must_be_unexplored ) const
{
    assert( dir != om_direction::type::invalid );

    if( !special.id ) {
        return false;
    }

    if( special.has_flag( "GLOBALLY_UNIQUE" ) &&
        overmap_buffer.contains_unique_special( special.id ) ) {
        return false;
    }

    const std::vector<overmap_special_locations> fixed_terrains = special.required_locations();

    return std::all_of( fixed_terrains.begin(), fixed_terrains.end(),
    [&]( const overmap_special_locations & elem ) {
        const tripoint_om_omt rp = p + om_direction::rotate( elem.p, dir );

        if( !inbounds( rp, 1 ) ) {
            return false;
        }

        if( must_be_unexplored ) {
            // If this must be unexplored, check if we've already got a submap generated.
            const bool existing_submap = is_omt_generated( rp );

            // If there is an existing submap, this area has already been explored and this
            // isn't a valid placement.
            if( existing_submap ) {
                return false;
            }
        }

        const oter_id &tid = ter( rp );

        return elem.can_be_placed_on( tid ) || ( rp.z() != 0 && tid == get_default_terrain( rp.z() ) );
    } );
}

// checks around the selected point to see if the special can be placed there
std::vector<tripoint_om_omt> overmap::place_special(
    const overmap_special &special, const tripoint_om_omt &p, om_direction::type dir,
    const city &cit, const bool must_be_unexplored, const bool force )
{
    assert( dir != om_direction::type::invalid );
    if( !force ) {
        assert( can_place_special( special, p, dir, must_be_unexplored ) );
    }

    if( special.has_flag( "GLOBALLY_UNIQUE" ) ) {
        overmap_buffer.add_unique_special( special.id );
    }

    const bool grid = special.has_flag( "ELECTRIC_GRID" );

    special_placement_result result = special.place( *this, p, dir );

    // Combine fixed connections with mutable
    for( const overmap_special_connection &elem : special.connections ) {
        const tripoint_om_omt rp = p + om_direction::rotate( elem.p, dir );
        cube_direction initial_dir = elem.initial_dir;
        if( initial_dir != cube_direction::last ) {
            initial_dir = initial_dir + dir;
        }
        result.cons_used.push_back( { elem.connection, { rp, initial_dir } } );
    }

    // Place connection
    for( const placed_connection &elem : result.cons_used ) {
        if( elem.connection ) {
            const tripoint_om_omt rp = elem.where.p;
            if( !elem.connection->can_start_at( ter( rp ) ) ) {
                continue;
            }

            const cube_direction initial_dir = elem.where.dir;

            bool linked = false;
            if( elem.connection->get_layout() == overmap_connection_layout::city && cit ) {
                // First, try to link to city, if layout allows that
                linked = build_connection( cit.pos, rp.xy(), rp.z(), *elem.connection,
                                           must_be_unexplored, initial_dir );
            }
            if( !linked && connection_cache ) {
                // If no city present, try to link to closest connection
                auto points = connection_cache->get_closests( elem.connection->id, rp.z(), rp.xy() );
                int attempts = 0;
                for( const point_om_omt &pos : points ) {
                    if( ( linked = build_connection( pos, rp.xy(), rp.z(), *elem.connection,
                                                     must_be_unexplored, initial_dir ) ) ||
                        ++attempts > 10 ) {
                        break;
                    }
                }
            }
            if( !linked && !connection_cache ) {
                // Cache is cleared once generation is done, if special is spawned via debug or for
                // mission we'll need to perform search. It is slow, but that's a rare case
                int attempts = 0;
                for( const tripoint_om_omt &p : closest_points_first( rp, OMAPX ) ) {
                    if( !inbounds( p ) ||
                        std::find( result.omts_used.begin(), result.omts_used.end(), p ) != result.omts_used.end() ||
                        !check_ot( elem.connection->id->default_terrain.str(), ot_match_type::type, p ) ) {
                        continue;
                    }
                    if( ( linked = build_connection( p.xy(), rp.xy(), rp.z(), *elem.connection,
                                                     must_be_unexplored, initial_dir ) ) ||
                        ++attempts > 10 ) {
                        break;
                    }
                }
            }
            if( !linked && initial_dir != cube_direction::last ) {
                // If nothing found, make a stub for a clean break, and also to connect other specials here later on
                pf::directed_path<point_om_omt> stub;
                stub.nodes.emplace_back( rp.xy(), om_direction::opposite( om_direction::from_cube( initial_dir,
                                         "Up and down connections not yet supported" ) ) );
                linked = build_connection( *elem.connection, stub, rp.z() );
            }
        }
    }

    // Link grid and mapgens
    const int args_index = mapgen_arg_storage.size();
    mapgen_arg_storage.emplace_back();
    for( const tripoint_om_omt &location : result.omts_used ) {
        mapgen_args_index[location] = args_index;
        overmap_special_placements[location] = special.id;
        if( grid ) {
            for( size_t i = 0; i < six_cardinal_directions.size(); i++ ) {
                const tripoint_om_omt other = location + six_cardinal_directions[i];
                if( std::find( result.omts_used.begin(), result.omts_used.end(),
                               other ) != result.omts_used.end() ) {
                    electric_grid_connections[location].set( i, true );
                }
            }
        }
    }

    // Place spawns.
    const overmap_special_spawns &spawns = special.get_monster_spawns();
    if( spawns.group ) {
        const int pop = rng( spawns.population.min, spawns.population.max );
        const int rad = rng( spawns.radius.min, spawns.radius.max );
        add_mon_group( mongroup( spawns.group, project_to<coords::sm>( p ), rad, pop ) );
    }

    // Place nested specials
    for( const auto &nested : special.get_nested_specials() ) {
        const tripoint_rel_omt shift = om_direction::rotate( nested.first, dir );
        const tripoint_om_omt rp = p + shift;
        if( can_place_special( *nested.second, rp, dir, false ) ) {
            std::vector<tripoint_om_omt> nested_result = place_special( *nested.second, rp, dir,
                    get_nearest_city( rp ), false, false );
            for( const auto &nested_point : nested_result ) {
                result.omts_used.push_back( nested_point + shift );
            }
        }
    }

    return result.omts_used;
}
// Inside empty rock spawn some ores maybe
void overmap::spawn_ores( const tripoint_abs_omt &p )
{
    //One in 15 at lowest level, goes up as you get closer to surface. Dig deep!
    if( one_in( 65 - abs( 5 * p.z() ) ) ) {
        std::string depth;
        switch( p.z() ) {
            case -1:
            case -2:
            case -3: {
                depth = "shallow";
                break;
            }
            case -4:
            case -5:
            case -6:
            case -7:  {
                depth = "medium";
                break;
            }
            case -8:
            case -9:
            case -10: {
                depth = "deep";
                break;
            }
            default:
            {depth = "how";}
        }
        weighted_int_list<std::string> ores;
        //ore_depth_to_rate in overmap.h
        for( const auto& [k, v] : ore_depth_to_rate.at( depth ) ) {
            ores.add( k, v );
        }
        std::string chosen = ores.pick()->c_str();
        std::vector<std::string> directions{"_north", "_east", "_south", "_west"};
        tripoint_om_omt local_pos = overmap_buffer.get_om_global( p ).local;
        const tripoint target_sub( omt_to_sm_copy( p.raw() ) );
        std::string note_text( chosen );
        std::ranges::replace( note_text, '_', ' ' );
        add_note( local_pos, string_format( "Signs of %s ore nearby.", note_text ) );
        if( !( MAPBUFFER.lookup_submap( target_sub ) ) ) {
            // No overmap to replace, set the terrain and bail.
            ter_set( local_pos, oter_id( "omt_ore_vein_" + chosen +
                                         directions[rand() % 4] ) );
            return;
        }
        /* Theres already overmap there, crap, hacky replace time!
        * begin edited editmap code TODO: Should probably just make this a
        * function, if there is one, I couldnt find it. Bascially "regenerates" an OM tile.
        */
        tinymap tmp;
        map &here = get_map();
        overmap_buffer.ter_set( p, oter_id( "omt_ore_vein_" + chosen + directions[rand() % 4] ) );
        tmp.generate( target_sub, calendar::turn );

        here.set_transparency_cache_dirty( p.z() );
        here.set_outside_cache_dirty( p.z() );
        here.set_floor_cache_dirty( p.z() );
        here.set_pathfinding_cache_dirty( p.z() );
        here.set_suspension_cache_dirty( p.z() );

        here.clear_vehicle_cache();
        here.clear_vehicle_list( p.z() );

        for( int x = 0; x < 2; x++ ) {
            for( int y = 0; y < 2; y++ ) {
                // Apply previewed mapgen to map. Since this is a function for testing, we try avoid triggering
                // functions that would alter the results
                const tripoint dest_pos = target_sub + point( x, y );
                const tripoint src_pos = tripoint{ x, y, p.z() };

                submap *destsm = MAPBUFFER.lookup_submap( dest_pos );
                submap *srcsm = tmp.get_submap_at_grid( src_pos );

                submap::swap( *destsm,  *srcsm );

                for( auto &veh : destsm->vehicles ) {
                    veh->sm_pos = dest_pos;
                }

                if( !destsm->spawns.empty() ) {                              // trigger spawnpoints
                    here.spawn_monsters( true );
                }
            }
        }

        // Since we cleared the vehicle cache of the whole z-level (not just the generate map), we add it back here
        for( int x = 0; x < here.getmapsize(); x++ ) {
            for( int y = 0; y < here.getmapsize(); y++ ) {
                const tripoint dest_pos = tripoint( x, y, p.z() );
                const submap *destsm = here.get_submap_at_grid( dest_pos );
                here.update_vehicle_list( destsm, p.z() ); // update real map's vcaches
            }
        }

        here.reset_vehicle_cache();
        /* end edited editmap code*/
    }
}

// Points list maintaining custom order, and allowing fast removal by coordinates
struct specials_overlay {
    std::list<tripoint_om_omt> order;
    std::list<tripoint_om_omt>::iterator lookup[OMAPX][OMAPY];

    specials_overlay( std::vector<tripoint_om_omt> &points ) {
        // Shuffle all points to randomly distribute specials across the overmap
        std::shuffle( points.begin(), points.end(), rng_get_engine() );

        order = { std::make_move_iterator( points.begin() ),
                  std::make_move_iterator( points.end() )
                };

        for( int x = 0; x < OMAPX; x++ ) {
            for( int y = 0; y < OMAPY; y++ ) {
                lookup[x][y] = order.end();
            }
        }
        for( auto it = order.begin(); it != order.end(); it++ ) {
            lookup[it->x()][it->y()] = it;
        }
    }

    std::list<tripoint_om_omt>::iterator begin() {
        return order.begin();
    }
    std::list<tripoint_om_omt>::iterator end() {
        return order.end();
    }

    void erase_other( const std::list<tripoint_om_omt>::iterator &i, const tripoint_om_omt &pos ) {
        if( !overmap::inbounds( pos ) || i->raw() == pos.raw() ) {
            return;
        }
        auto it = lookup[pos.x()][pos.y()];
        if( it != order.end() ) {
            lookup[pos.x()][pos.y()] = order.end();
            order.erase( it );
        }
    }
    std::list<tripoint_om_omt>::iterator erase_this( const std::list<tripoint_om_omt>::iterator &i ) {
        auto it = lookup[i->x()][i->y()];
        if( it != order.end() ) {
            lookup[i->x()][i->y()] = order.end();
            return order.erase( it );
        }
        return i;
    }
};

int overmap::place_special_custom( const overmap_special &special,
                                   std::vector<tripoint_om_omt> &points )
{
    std::unique_ptr<specials_overlay> target = std::make_unique<specials_overlay>( points );
    return place_special_attempt( special, 1, *target, true );
}

int overmap::place_special_attempt( const overmap_special &special, const int max,
                                    specials_overlay &points, const bool must_be_unexplored )
{
    if( max < 1 ) {
        return 0;
    }

    const int RANGE = get_option<int>( "SPECIALS_SPACING" );

    // Check how many cities are suitable for this specials
    // and try to distribute specials between then
    bool need_city = special.requires_city();
    int max_per_city = INT_MAX;
    std::unordered_map<const city *, int> valid_city;
    if( need_city ) {
        int valid_cities = 0;
        for( const auto &city : cities ) {
            if( special.get_constraints().city_size.contains( city.size ) ) {
                valid_cities++;
                valid_city[&city] = 0;
            }
        }
        if( valid_cities < 1 ) {
            return 0;
        }
        max_per_city = std::ceil( static_cast<float>( max ) / valid_cities );
    }

    int placed = 0;
    for( auto p = points.begin(); p != points.end(); ) {
        const city &nearest_city = get_nearest_city( *p );

        // City check is the fastest => it goes first.
        if( need_city ) {
            if( !valid_city.contains( &nearest_city ) ||
                valid_city[&nearest_city] >= max_per_city ||
                !special.can_belong_to_city( *p, nearest_city ) ) {
                p++;
                continue;
            }
        }

        // See if we can actually place the special there.
        const auto rotation = random_special_rotation( special, *p, must_be_unexplored );
        if( rotation == om_direction::type::invalid ) {
            p++;
            continue;
        }
        std::vector<tripoint_om_omt> result = place_special( special, *p, rotation, nearest_city, false,
                                              must_be_unexplored );
        if( need_city ) {
            valid_city[&nearest_city]++;
        }

        // Remove all used points from our candidates list
        if( RANGE == 0 ) {
            for( const auto &dest : result ) {
                if( dest.z() == 0 ) {
                    points.erase_other( p, dest );
                }
            }
        } else if( RANGE > 0 ) {
            tripoint_om_omt p_min = { INT_MAX, INT_MAX, 0 };
            tripoint_om_omt p_max = { INT_MIN, INT_MIN, 0 };
            for( const auto &dest : result ) {
                if( dest.z() == 0 ) {
                    p_min.raw().x = std::min( p_min.x(), dest.x() );
                    p_max.raw().x = std::max( p_max.x(), dest.x() );
                    p_min.raw().y = std::min( p_min.y(), dest.y() );
                    p_max.raw().y = std::max( p_max.y(), dest.y() );
                }
            }
            const point_rel_omt shift( RANGE, RANGE );
            for( const tripoint_om_omt &dest : tripoint_range<tripoint_om_omt>( p_min - shift,
                    p_max + shift ) ) {
                points.erase_other( p, dest );
            }
        }

        // Sometimes points can be reused with different rotation, wa want to avoid
        // that for better dispertion - make sure current origin is removed even if
        // this special doesn't place any surface omt
        p = points.erase_this( p );

        if( ++placed >= max ) {
            break;
        }
    }
    return placed;
}

// Iterate over overmap searching for valid locations, and placing specials
void overmap::place_specials( overmap_special_batch &enabled_specials )
{
    const int RANGE = std::max( 0, get_option<int>( "SPECIALS_SPACING" ) );
    const float DENSITY = get_option<float>( "SPECIALS_DENSITY" );

    static const overmap_location_id water( "water" );

    // We have four zones with individual point pools:
    // land surface, land underground, all lakes, all rivers
    // not very consistent, but we don't have any underwater specials to bother
    enum zone : int {
        land,
        land_under,
        lake,
        river,
        last
    };

    struct area {
        int surface = 0;
        int under = 0;
    };

    // Rough estimate how many space our specials about to take with
    // our range, density, and available specials, if we can't possibly
    // have that much - tune density down
    float area_needed[zone::last] = { 0 };

    // We'll need to track locations of land surface specials
    cata::flat_set<overmap_location_id> land_locs;

    std::unordered_map<overmap_special_id, zone> special_zone;
    std::unordered_map<overmap_special_id, int> special_area;
    for( auto &iter : enabled_specials ) {
        const overmap_special &special = *iter.special_details;
        const std::vector<overmap_special_locations> &locs = special.required_locations();

        // Check all locations to find out if that's river or underground special
        cata::flat_set<overmap_location_id> this_locs;
        area this_area;
        for( const overmap_special_locations &loc : locs ) {
            if( loc.p.z == 0 ) {
                this_area.surface++;
                // Only z0 locations are actually matched, other ones are ignored
                this_locs.insert( loc.locations.begin(), loc.locations.end() );
            } else {
                this_area.under++;
            }
        }

        zone current = special.has_flag( "LAKE" ) ? zone::lake :
                       special.has_flag( "RIVER" ) ? zone::river :
                       this_locs.count( water ) ? zone::river :
                       this_area.surface ? zone::land : zone::land_under;

        if( current == zone::land ) {
            land_locs.insert( this_locs.begin(), this_locs.end() );
        }

        const numeric_interval<int> &o = special.get_constraints().occurrences;

        float average_amount = special.has_flag( "UNIQUE" ) ?
                               static_cast<float>( o.min ) / o.max :
                               static_cast<float>( o.min + o.max ) / 2;

        special_area[special.id] = current == zone::land_under ? this_area.under : this_area.surface;
        float this_range = current == zone::land_under ? 0 : RANGE;
        float total_area = std::pow( std::sqrt( special_area[special.id] ) + this_range, 2.0 ) *
                           average_amount * DENSITY;

        area_needed[current] += total_area;
        special_zone[special.id] = current;
    }

    // Sort specials be they sizes - placing big things is faster
    // and easier while we have most of map still empty, and also
    // that central lab will have top priority
    bool is_true_center = pos() == point_abs_om();
    const auto special_weight = [&]( const overmap_special * s ) {
        int weight = special_area[s->id];
        if( is_true_center && s->has_flag( "ENDGAME" ) ) {
            weight *= 1000;
        }
        return weight;
    };
    std::sort( enabled_specials.begin(), enabled_specials.end(),
    [&special_weight]( const overmap_special_placement & a, const overmap_special_placement & b ) {
        return special_weight( a.special_details ) > special_weight( b.special_details );
    } );

    // Prepare vectors of points for all zones
    std::vector<tripoint_om_omt> zone_points[zone::last];
    for( int x = 0; x < OMAPX; x++ ) {
        for( int y = 0; y < OMAPY; y++ ) {
            const tripoint_om_omt p = {x, y, 0};
            const oter_id &t = ter( p );

            zone current = t->is_lake() ? zone::lake :
                           t->is_river() ? zone::river :
                           zone::land_under;

            // Grab all lakes, rivers, and undergrounds
            zone_points[current].push_back( p );

            if( current == zone::land_under && is_amongst_locations( t, land_locs ) ) {
                // Points above undergrounds goes to land zone, but only if this location
                // can be used by some special, this way we'll filter out city buildings
                zone_points[zone::land].push_back( p );
            }
        }
    }

    static const float OMAP_AREA = static_cast<float>( OMAPX * OMAPY );
    float zone_ratio[zone::last];
    for( int i = 0; i < zone::river; i++ ) {
        // Most of the setups should end with x1 multiplier, but some dire combinations
        // of settings like maxed spacing and density may actually need adjusting to
        // give a things on bottom of the list a chance to spawn
        float crowd_ratio = std::min( 1.0f, OMAP_AREA / area_needed[i] );

        // Calculate terrain ratio to normalize specials occurencies
        // we don't want to dump all specials on overmap covered by water
        zone_ratio[i] = ( zone_points[i].size() / OMAP_AREA ) * crowd_ratio * DENSITY;
    }
    // TODO: Yes, that's a hack. Calculatng real river ratio would require rebalancing occurences.
    zone_ratio[zone::river] = zone_ratio[zone::land_under];

    std::unique_ptr<specials_overlay> zone_overlay[zone::last];
    for( int i = 0; i < zone::last; i++ ) {
        zone_overlay[i] = std::make_unique<specials_overlay>( zone_points[i] );
    }

    // Now all preparations is done, and we can start placing specials
    for( auto &iter : enabled_specials ) {
        const overmap_special &special = *iter.special_details;
        const overmap_special_placement_constraints &constraints = special.get_constraints();

        const int min = constraints.occurrences.min;
        const int max = constraints.occurrences.max;

        zone current = special_zone[special.id];

        const float rate = is_true_center && special.has_flag( "ENDGAME" ) ? 1 :
                           zone_ratio[current];

        const bool unique = iter.special_details->has_flag( "UNIQUE" );
        const bool globally_unique = iter.special_details->has_flag( "GLOBALLY_UNIQUE" );

        int amount_to_place;
        if( unique || globally_unique ) {
            const overmap_special_id &id = iter.special_details->id;

            //FINGERS CROSSED EMOGI
            amount_to_place = x_in_y( min, max ) && ( !globally_unique ||
                              !overmap_buffer.contains_unique_special( id ) ) ? 1 : 0;
        } else {
            // Number of instances normalized to terrain ratio
            float real_max = std::max( static_cast<float>( min ), max * rate );
            amount_to_place = roll_remainder( rng_float( min, real_max ) );
        }
        iter.instances_placed += place_special_attempt( special,
                                 amount_to_place, *zone_overlay[current], false );
    }
}

void overmap::place_mongroups()
{
    // Cities are full of zombies
    for( const city &elem : cities ) {
        if( get_option<bool>( "WANDER_SPAWNS" ) ) {
            if( !one_in( 16 ) || elem.size > 5 ) {
                mongroup m( GROUP_ZOMBIE,
                            tripoint_om_sm( project_to<coords::sm>( elem.pos ), 0 ),
                            static_cast<int>( elem.size * 2.5 ),
                            elem.size * 80 );
                //                m.set_target( zg.back().posx, zg.back().posy );
                m.horde = true;
                m.wander( *this );
                add_mon_group( m );
            }
        }
    }

    if( pos() == point_abs_om() ) {
        // Figure out where the dimensional lab is, and flood area with nether critters
        for( int x = 0; x < OMAPX; x++ ) {
            for( int y = 0; y < OMAPY; y++ ) {
                tripoint_om_omt p( x, y, 0 );
                if( ter( p ) == "central_lab_entrance" ) {
                    add_mon_group( mongroup( GROUP_DIMENSIONAL_SURFACE, project_to<coords::sm>( p ), 5, 30 ) );
                }
            }
        }
    }

    // Place the "put me anywhere" groups
    int numgroups = rng( 0, 3 );
    for( int i = 0; i < numgroups; i++ ) {
        add_mon_group( mongroup( GROUP_WORM, tripoint( rng( 0, OMAPX * 2 - 1 ), rng( 0,
                                 OMAPY * 2 - 1 ), 0 ),
                                 rng( 20, 40 ), rng( 30, 50 ) ) );
    }
}

void overmap::place_nemesis( const tripoint_abs_omt p )
{
    tripoint_abs_sm pos_sm = project_to<coords::sm>( p );
    point_abs_om omp;
    tripoint_om_sm local_sm;
    std::tie( omp, local_sm ) = project_remain<coords::om>( pos_sm );

    mongroup nemesis( GROUP_NEMESIS, local_sm, 1, 1 );
    nemesis.horde = true;
    nemesis.horde_behaviour = "nemesis";
    nemesis.abs_pos = pos_sm;
    add_mon_group( nemesis );
}

point_abs_omt overmap::global_base_point() const
{
    return project_to<coords::omt>( loc );
}

void overmap::place_radios()
{
    auto strength = []() {
        return rng( RADIO_MIN_STRENGTH, RADIO_MAX_STRENGTH );
    };
    std::string message;
    for( int i = 0; i < OMAPX; i++ ) {
        for( int j = 0; j < OMAPY; j++ ) {
            tripoint_om_omt pos_omt( i, j, 0 );
            point_om_sm pos_sm = project_to<coords::sm>( pos_omt.xy() );

            // Since location have id such as "radio_tower_1_north", we must check the beginning of the id
            if( is_ot_match( "radio_tower", ter( pos_omt ), ot_match_type::prefix ) ) {
                if( one_in( 3 ) ) {
                    radios.emplace_back( pos_sm, strength(), "", radio_type::WEATHER_RADIO );
                } else {
                    message = SNIPPET.expand( SNIPPET.random_from_category( "radio_archive" ).value_or(
                                                  translation() ).translated() );
                    radios.emplace_back( pos_sm, strength(), message );
                }
            } else if( is_ot_match( "lmoe", ter( pos_omt ), ot_match_type::prefix ) ) {
                message = string_format( _( "This is automated emergency shelter beacon %d%d."
                                            "  Supplies, amenities and shelter are stocked." ), i, j );
                radios.emplace_back( pos_sm, strength() / 2, message );
            } else if( is_ot_match( "fema_entrance", ter( pos_omt ), ot_match_type::prefix ) ) {
                message = string_format( _( "This is FEMA camp %d%d."
                                            "  Supplies are limited, please bring supplemental food, water, and bedding."
                                            "  This is FEMA camp %d%d.  A designated long-term emergency shelter." ), i, j, i, j );
                radios.emplace_back( pos_sm, strength(), message );
            } else if( ter( pos_omt ) == "central_lab_entrance" && pos() == point_abs_om() ) {
                std::string message =
                    _( "If you can hear this message, the probe to 021XC is functioning correctly." );
                // Repeat the message on different frequencies
                for( int i = 0; i < 10; i++ ) {
                    radios.emplace_back( pos_sm, RADIO_MAX_STRENGTH, message );
                }
            }
        }
    }
}

void overmap::open( overmap_special_batch &enabled_specials )
{
    // const std::string terfilename = overmapbuffer::terrain_filename( loc );

    const auto ter_reader = [&]( std::istream & fin ) {
        overmap::unserialize( fin, string_format( "overmap terrain %d.%d", loc.x(), loc.y() ) );
    };

    if( g->get_active_world()->read_overmap( loc, ter_reader ) ) {
        // const std::string plrfilename = overmapbuffer::player_filename( loc );
        const auto plr_reader = [&]( std::istream & fin ) {
            overmap::unserialize_view( fin, string_format( "overmap visibility %d.%d", loc.x(), loc.y() ) );
        };
        g->get_active_world()->read_overmap_player_visibility( loc, plr_reader );
    } else { // No map exists!  Prepare neighbors, and generate one.
        std::vector<const overmap *> pointers;
        // Fetch south and north
        for( int i = -1; i <= 1; i += 2 ) {
            pointers.push_back( overmap_buffer.get_existing( loc + point( 0, i ) ) );
        }
        // Fetch east and west
        for( int i = -1; i <= 1; i += 2 ) {
            pointers.push_back( overmap_buffer.get_existing( loc + point( i, 0 ) ) );
        }

        // pointers looks like (north, south, west, east)
        generate( pointers[0], pointers[3], pointers[1], pointers[2], enabled_specials );
    }
}

// Note: this may throw io errors from std::ofstream
void overmap::save() const
{
    g->get_active_world()->write_overmap_player_visibility( loc, [&]( std::ostream & stream ) {
        serialize_view( stream );
    } );

    g->get_active_world()->write_overmap( loc, [&]( std::ostream & stream ) {
        serialize( stream );
    } );
}

void overmap::add_mon_group( const mongroup &group )
{
    // Monster groups: the old system had large groups (radius > 1),
    // the new system transforms them into groups of radius 1, this also
    // makes the diffuse setting obsolete (as it only controls how the radius
    // is interpreted) - it's only used when adding monster groups with function.
    if( group.radius == 1 ) {
        zg.insert( std::pair<tripoint_om_sm, mongroup>( group.pos, group ) );
        return;
    }
    // diffuse groups use a circular area, non-diffuse groups use a rectangular area
    const int rad = std::max<int>( 0, group.radius );
    const double total_area = group.diffuse ? std::pow( rad + 1, 2 ) : ( rad * rad * M_PI + 1 );
    const double pop = std::max<int>( 0, group.population );
    for( int x = -rad; x <= rad; x++ ) {
        for( int y = -rad; y <= rad; y++ ) {
            const int dist = group.diffuse ? square_dist( point( x, y ), point_zero ) : trig_dist( point( x,
                             y ), point_zero );
            if( dist > rad ) {
                continue;
            }
            // Population on a single submap, *not* a integer
            double pop_here;
            if( rad == 0 ) {
                pop_here = pop;
            } else if( group.diffuse ) {
                pop_here = pop / total_area;
            } else {
                // non-diffuse groups are more dense towards the center.
                // This computation is delicate, be careful and see
                // https://github.com/CleverRaven/Cataclysm-DDA/issues/26941
                pop_here = ( static_cast<double>( rad - dist ) / rad ) * pop / total_area;
            }
            if( pop_here > pop || pop_here < 0 ) {
                dbg( DL::Error ) << "overmap::add_mon_group: " << group.type.str()
                                 << " - invalid population here: " << pop_here;
            }
            int p = std::max( 0, static_cast<int>( std::floor( pop_here ) ) );
            if( pop_here - p != 0 ) {
                // in case the population is something like 0.2, randomly add a
                // single population unit, this *should* on average give the correct
                // total population.
                const int mod = static_cast<int>( 10000.0 * ( pop_here - p ) );
                if( x_in_y( mod, 10000 ) ) {
                    p++;
                }
            }
            if( p == 0 ) {
                continue;
            }
            // Exact copy to keep all important values, only change what's needed
            // for a single-submap group.
            mongroup tmp( group );
            tmp.radius = 1;
            tmp.pos += point( x, y );
            tmp.population = p;
            // This *can* create groups outside of the area of this overmap.
            // As this function is called during generating the overmap, the
            // neighboring overmaps might not have been generated and one can't access
            // them through the overmapbuffer as this would trigger generating them.
            // This would in turn to lead to a call to this function again.
            // To avoid this, the overmapbuffer checks the monster groups when loading
            // an overmap and moves groups with out-of-bounds position to another overmap.
            add_mon_group( tmp );
        }
    }
}

void overmap::for_each_npc( const std::function<void( npc & )> &callback )
{
    for( auto &guy : npcs ) {
        callback( *guy );
    }
}

void overmap::for_each_npc( const std::function<void( const npc & )> &callback ) const
{
    for( auto &guy : npcs ) {
        callback( *guy );
    }
}

shared_ptr_fast<npc> overmap::find_npc( const character_id &id ) const
{
    for( const auto &guy : npcs ) {
        if( guy->getID() == id ) {
            return guy;
        }
    }
    return nullptr;
}

bool overmap::is_omt_generated( const tripoint_om_omt &loc ) const
{
    if( !inbounds( loc ) ) {
        return false;
    }

    // Location is local to this overmap, but we need global submap coordinates
    // for the mapbuffer lookup.
    tripoint_abs_sm global_sm_loc =
        project_to<coords::sm>( project_combine( pos(), loc ) );

    // TODO: fix point types
    const bool is_generated = MAPBUFFER.lookup_submap( global_sm_loc.raw() ) != nullptr;

    return is_generated;
}

void overmap::set_electric_grid_connections( const tripoint_om_omt &p,
        const std::bitset<six_cardinal_directions.size()> &connections )
{
    electric_grid_connections[p] = connections;
    for( size_t i = 0; i < six_cardinal_directions.size(); i++ ) {
        tripoint_om_omt other_p = p + six_cardinal_directions[i];
        tripoint_abs_omt other_p_global = project_combine( pos(), other_p );
        overmap_with_local_coords other = overmap_buffer.get_om_global( other_p_global );
        size_t opposite_direction = i + ( ( i % 2 ) ? -1 : 1 );
        other.om->electric_grid_connections[other.local][opposite_direction] = connections[i];
    }
}

overmap_special_id overmap_specials::create_building_from( const oter_type_str_id &base )
{
    // TODO: Get rid of the hard-coded ids.
    static const overmap_location_id land( "land" );
    static const overmap_location_id swamp( "swamp" );

    overmap_special_terrain ter;
    ter.terrain = base.obj().get_first().id();
    ter.locations.insert( land );
    ter.locations.insert( swamp );

    overmap_special_id new_id( "FakeSpecial_" + base.str() );
    overmap_special new_special( new_id, ter );

    new_special.set_flag( "ELECTRIC_GRID" );

    return specials.insert( new_special ).id;
}

namespace io
{
template<>
std::string enum_to_string<ot_match_type>( ot_match_type data )
{
    switch( data ) {
        // *INDENT-OFF*
        case ot_match_type::exact: return "EXACT";
        case ot_match_type::type: return "TYPE";
        case ot_match_type::prefix: return "PREFIX";
        case ot_match_type::contains: return "CONTAINS";
        // *INDENT-ON*
        case ot_match_type::num_ot_match_type:
            break;
    }
    debugmsg( "Invalid ot_match_type" );
    abort();
}
} // namespace io

constexpr tripoint_abs_omt overmap::invalid_tripoint;

std::string oter_no_dir( const oter_id &oter )
{
    std::string base_oter_id = oter.id().c_str();
    size_t oter_len = base_oter_id.size();
    if( oter_len > 7 ) {
        if( base_oter_id.substr( oter_len - 6, 6 ) == "_south" ) {
            return base_oter_id.substr( 0, oter_len - 6 );
        } else if( base_oter_id.substr( oter_len - 6, 6 ) == "_north" ) {
            return base_oter_id.substr( 0, oter_len - 6 );
        }
    }
    if( oter_len > 6 ) {
        if( base_oter_id.substr( oter_len - 5, 5 ) == "_west" ) {
            return base_oter_id.substr( 0, oter_len - 5 );
        } else if( base_oter_id.substr( oter_len - 5, 5 ) == "_east" ) {
            return base_oter_id.substr( 0, oter_len - 5 );
        }
    }
    return base_oter_id;
}

om_direction::type oter_get_rotation_dir( const oter_id &oter )
{
    for( const om_direction::type &rot : om_direction::all ) {
        const std::string &rot_s = om_direction::get_suffix( rot );
        if( oter.id().str().ends_with( rot_s ) ) {
            return rot;
        }
    }
    return om_direction::type::invalid;
}

int oter_get_rotations( const oter_id &oter )
{
    return om_direction::get_num_cw_rotations( oter_get_rotation_dir( oter ) );
}

const std::string &oter_get_rotation_string( const oter_id &oter )
{
    return om_direction::get_suffix( oter_get_rotation_dir( oter ) );
}

bool belongs_to_connection( const overmap_connection_id &id, const oter_id &oter )
{
    return is_ot_match( id->default_terrain.str(), oter, ot_match_type::type );
}
