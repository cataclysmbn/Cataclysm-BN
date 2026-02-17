#include "mapdata.h"

#include <algorithm>
#include <cstdlib>
#include <iterator>
#include <map>
#include <memory>
#include <unordered_map>
#include <utility>

#include "assign.h"
#include "calendar.h"
#include "cached_options.h"
#include "color.h"
#include "debug.h"
#include "enum_conversions.h"
#include "generic_factory.h"
#include "harvest.h"
#include "iexamine.h"
#include "int_id.h"
#include "item.h"
#include "item_group.h"
#include "json.h"
#include "make_static.h"
#include "output.h"
#include "rng.h"
#include "string_formatter.h"
#include "string_id.h"
#include "translations.h"
#include "trap.h"
#include "type_id.h"

static const std::string flag_TRANSPARENT( "TRANSPARENT" );

static void set_furn_ids();

namespace
{

const units::volume DEFAULT_MAX_VOLUME_IN_SQUARE = units::from_liter( 1000 );

generic_factory<ter_t> terrain_data( "terrain" );
generic_factory<furn_t> furniture_data( "furniture" );

bool is_json_check_strict( const std::string &src )
{
    return json_report_strict || is_strict_enabled( src );
}

} // namespace

/** @relates int_id */
template<>
bool int_id<ter_t>::is_valid() const
{
    return terrain_data.is_valid( *this );
}

/** @relates int_id */
template<>
const ter_t &int_id<ter_t>::obj() const
{
    return terrain_data.obj( *this );
}

/** @relates int_id */
template<>
const string_id<ter_t> &int_id<ter_t>::id() const
{
    return terrain_data.convert( *this );
}

/** @relates int_id */
template<>
int_id<ter_t> string_id<ter_t>::id() const
{
    return terrain_data.convert( *this, t_null );
}

/** @relates int_id */
template<>
int_id<ter_t>::int_id( const string_id<ter_t> &id ) : _id( id.id() )
{
}

/** @relates string_id */
template<>
const ter_t &string_id<ter_t>::obj() const
{
    return terrain_data.obj( *this );
}

/** @relates string_id */
template<>
bool string_id<ter_t>::is_valid() const
{
    return terrain_data.is_valid( *this );
}

/** @relates int_id */
template<>
bool int_id<furn_t>::is_valid() const
{
    return furniture_data.is_valid( *this );
}

/** @relates int_id */
template<>
const furn_t &int_id<furn_t>::obj() const
{
    return furniture_data.obj( *this );
}

/** @relates int_id */
template<>
const string_id<furn_t> &int_id<furn_t>::id() const
{
    return furniture_data.convert( *this );
}

/** @relates string_id */
template<>
bool string_id<furn_t>::is_valid() const
{
    return furniture_data.is_valid( *this );
}

/** @relates string_id */
template<>
const furn_t &string_id<furn_t>::obj() const
{
    return furniture_data.obj( *this );
}

/** @relates string_id */
template<>
int_id<furn_t> string_id<furn_t>::id() const
{
    return furniture_data.convert( *this, f_null );
}

/** @relates int_id */
template<>
int_id<furn_t>::int_id( const string_id<furn_t> &id ) : _id( id.id() )
{
}

static const std::unordered_map<std::string, ter_bitflags> ter_bitflags_map = { {
        { "DESTROY_ITEM",             TFLAG_DESTROY_ITEM },   // add/spawn_item*()
        { "ROUGH",                    TFLAG_ROUGH },          // monmove
        { "UNSTABLE",                 TFLAG_UNSTABLE },       // monmove
        { "LIQUID",                   TFLAG_LIQUID },         // *move(), add/spawn_item*()
        { "FIRE_CONTAINER",           TFLAG_FIRE_CONTAINER }, // fire
        { "SUPPRESS_SMOKE",           TFLAG_SUPPRESS_SMOKE }, // fire
        { "FLAMMABLE_HARD",           TFLAG_FLAMMABLE_HARD }, // fire
        { "SEALED",                   TFLAG_SEALED },         // Fire, acid
        { "ALLOW_FIELD_EFFECT",       TFLAG_ALLOW_FIELD_EFFECT }, // Fire, acid
        { "COLLAPSES",                TFLAG_COLLAPSES },      // This tile includes a ceiling. If the ceiling drops, this tile is destroyed.
        { "FLAMMABLE",                TFLAG_FLAMMABLE },      // fire bad! fire SLOW!
        { "REDUCE_SCENT",             TFLAG_REDUCE_SCENT },   // ...and the other half is update_scent
        { "INDOORS",                  TFLAG_INDOORS },        // vehicle gain_moves, weather
        { "SHARP",                    TFLAG_SHARP },          // monmove
        { "SUPPORTS_ROOF",            TFLAG_SUPPORTS_ROOF },  // Supports its ceiling and roof above it.
        { "MINEABLE",                 TFLAG_MINEABLE },       // allows mining
        { "SWIMMABLE",                TFLAG_SWIMMABLE },      // monmove, many fields
        { "TRANSPARENT",              TFLAG_TRANSPARENT },    // map::is_transparent / lightmap
        { "NOITEM",                   TFLAG_NOITEM },         // add/spawn_item*()
        { "NO_SIGHT",                 TFLAG_NO_SIGHT },       // Sight reduced to 1 on this tile
        { "FLAMMABLE_ASH",            TFLAG_FLAMMABLE_ASH },  // oh hey fire. again.
        { "WALL",                     TFLAG_WALL },           // Badly defined. Used for roof support, mapgen, and fungalization result.
        { "NO_SCENT",                 TFLAG_NO_SCENT },       // cannot have scent values, which prevents scent diffusion through this tile
        { "DEEP_WATER",               TFLAG_DEEP_WATER },     // Deep enough to submerge things
        { "WATER_CUBE",               TFLAG_WATER_CUBE },     // Water tile that is entirely water
        { "CURRENT",                  TFLAG_CURRENT },        // Water is flowing.
        { "HARVESTED",                TFLAG_HARVESTED },      // harvested.  will not bear fruit.
        { "PERMEABLE",                TFLAG_PERMEABLE },      // gases can flow through.
        { "AUTO_WALL_SYMBOL",         TFLAG_AUTO_WALL_SYMBOL }, // automatically create the appropriate wall
        { "CONNECT_TO_WALL",          TFLAG_CONNECT_TO_WALL }, // superseded by ter_connects, retained for json backward compatibility
        { "CLIMBABLE",                TFLAG_CLIMBABLE },      // Can be climbed over
        { "GOES_DOWN",                TFLAG_GOES_DOWN },      // Allows non-flying creatures to move downwards
        { "GOES_UP",                  TFLAG_GOES_UP },        // Allows non-flying creatures to move upwards
        { "NO_FLOOR",                 TFLAG_NO_FLOOR },       // Things should fall when placed on this tile
        { "SEEN_FROM_ABOVE",          TFLAG_SEEN_FROM_ABOVE },// This should be visible if the tile above has no floor
        { "HIDE_PLACE",               TFLAG_HIDE_PLACE },     // Creature on this tile can't be seen by other creature not standing on adjacent tiles
        { "BLOCK_WIND",               TFLAG_BLOCK_WIND },     // This tile will partially block the wind.
        { "FLAT",                     TFLAG_FLAT },           // This tile is flat.
        { "RAMP",                     TFLAG_RAMP },           // Can be used to move up a z-level
        { "RAMP_DOWN",                TFLAG_RAMP_DOWN },      // Anything entering this tile moves down a z-level
        { "RAMP_UP",                  TFLAG_RAMP_UP },        // Anything entering this tile moves up a z-level
        { "RAIL",                     TFLAG_RAIL },           // Rail tile (used heavily)
        { "THIN_OBSTACLE",            TFLAG_THIN_OBSTACLE },  // Passable by players and monsters. Vehicles destroy it.
        { "SMALL_PASSAGE",            TFLAG_SMALL_PASSAGE },   // A small passage, that large or huge things cannot pass through
        { "Z_TRANSPARENT",            TFLAG_Z_TRANSPARENT },  // Doesn't block vision passing through the z-level
        { "SUN_ROOF_ABOVE",           TFLAG_SUN_ROOF_ABOVE }, // This furniture has a "fake roof" above, that blocks sunlight (see #44421).
        { "FUNGUS",                   TFLAG_FUNGUS },         // Marks terrain/furniture as infected with fungus
        { "FLOWER",                   TFLAG_FLOWER },         // Is flower
        { "ORGANIC",                  TFLAG_ORGANIC },        // Is organic (e.g. flower)
        { "PLANT",                    TFLAG_PLANT },          // Is fungal stalk
        { "SHRUB",                    TFLAG_SHRUB },          // Is shrub
        { "TREE",                     TFLAG_TREE },           // Is tree
        { "SUSPENDED",                TFLAG_SUSPENDED },      // This furniture is suspended between other terrain, and will cause a cascading failure on break.
        { "FRIDGE",                   TFLAG_FRIDGE },         // This is an active fridge.
        { "FREEZER",                  TFLAG_FREEZER },        // This is an active freezer.
        { "ELEVATOR",                 TFLAG_ELEVATOR },       // This is an elevator.
        { "NO_MEMORY",                TFLAG_NO_MEMORY },      // This should not be added to map memory
    }
};

static const std::unordered_map<std::string, ter_connects> ter_connects_map = { {
        { "WALL",                     TERCONN_WALL },         // implied by TFLAG_CONNECT_TO_WALL, TFLAG_AUTO_WALL_SYMBOL or TFLAG_WALL
        { "CHAINFENCE",               TERCONN_CHAINFENCE },
        { "WOODFENCE",                TERCONN_WOODFENCE },
        { "RAILING",                  TERCONN_RAILING },
        { "WATER",                    TERCONN_WATER },
        { "PAVEMENT",                 TERCONN_PAVEMENT },
        { "RAIL",                     TERCONN_RAIL },
        { "GUTTER",                     TERCONN_GUTTER },
        { "COUNTER",                     TERCONN_COUNTER },
    }
};

void ranged_bash_info::deserialize( JsonIn &jsin )
{
    JsonObject jo = jsin.get_object();

    assign( jo, "reduction", reduction );
    assign( jo, "reduction_laser", reduction_laser );
    assign( jo, "destroy_threshold", destroy_threshold );
    assign( jo, "flammable", flammable );
    assign( jo, "block_unaimed_chance", block_unaimed_chance );
}

static void load_map_bash_tent_centers( const JsonArray &ja, std::vector<furn_str_id> &centers )
{
    for( const std::string &line : ja ) {
        centers.emplace_back( line );
    }
}

static void correct_if_magic( std::optional<int> &val )
{
    if( val && *val < 0 ) {
        val.reset();
    }
}

map_bash_info::map_bash_info() : str_min( -1 ), str_max( -1 ),
    str_min_blocked( -1 ), str_max_blocked( -1 ),
    str_min_supported( -1 ), str_max_supported( -1 ),
    explosive( 0 ), sound_vol( -1 ), sound_fail_vol( -1 ),
    collapse_radius( 1 ), destroy_only( false ), bash_below( false ),
    drop_group( "EMPTY_GROUP" ),
    ter_set( ter_str_id::NULL_ID() ), furn_set( furn_str_id::NULL_ID() ) {}

void map_bash_info::deserialize( JsonIn &jsin )
{
    JsonObject jo = jsin.get_object();

    assign( jo, "str_min", str_min );
    assign( jo, "str_max", str_max );

    assign( jo, "str_min_blocked", str_min_blocked );
    assign( jo, "str_max_blocked", str_max_blocked );

    assign( jo, "str_min_supported", str_min_supported );
    assign( jo, "str_max_supported", str_max_supported );

    assign( jo, "explosive", explosive );

    assign( jo, "sound_vol", sound_vol );
    correct_if_magic( sound_vol );

    assign( jo, "sound_fail_vol", sound_fail_vol );
    correct_if_magic( sound_fail_vol );

    assign( jo, "collapse_radius", collapse_radius );

    assign( jo, "destroy_only", destroy_only );

    assign( jo, "bash_below", bash_below );

    assign( jo, "sound", sound );
    assign( jo, "sound_fail", sound_fail );

    assign( jo, "furn_set", furn_set );

    assign( jo, "ter_set", ter_set );
    assign( jo, "ter_set_bashed_from_above", ter_set_bashed_from_above );

    assign( jo, "move_cost", fd_bash_move_cost );
    assign( jo, "msg_success", field_bash_msg_success );

    if( jo.has_member( "items" ) ) {
        drop_group = item_group::load_item_group( jo.get_member( "items" ), "collection" );
    }

    if( jo.has_array( "tent_centers" ) ) {
        load_map_bash_tent_centers( jo.get_array( "tent_centers" ), tent_centers );
    }

    assign( jo, "ranged", ranged );
}

void map_bash_info::finalize()
{
    if( !ter_set_bashed_from_above ) {
        ter_set_bashed_from_above = ter_set;
    }
}

static const std::string &map_object_type_to_str( map_bash_info::map_object_type type )
{
    switch( type ) {
        case map_bash_info::map_object_type::terrain:
            return STATIC( "terrain" );
        case map_bash_info::map_object_type::furniture:
            return STATIC( "furniture" );
        case map_bash_info::map_object_type::field:
            return STATIC( "field" );
        default:
            break;
    }

    return STATIC( "ERROR!" );
}

void map_bash_info::check( const std::string &id, map_object_type type ) const
{
    std::vector<std::string> errors;
    if( type != map_bash_info::map_object_type::furniture && furn_set ) {
        errors.emplace_back( _( "\"furn_set\" is set" ) );
    }

    if( type != map_bash_info::map_object_type::terrain && ter_set ) {
        errors.emplace_back( _( "\"ter_set\" is set" ) );
    }

    if( type != map_bash_info::map_object_type::terrain && ter_set_bashed_from_above ) {
        errors.emplace_back( _( "\"ter_set_bashed_from_above\" is set" ) );
    }

    if( !errors.empty() ) {
        const std::string &type_str = map_object_type_to_str( type );
        debugmsg( _( "Errors for \"bash\" field in \"%s\": \"%s\":\n%s" ),
                  type_str, id,
                  enumerate_as_string( errors, enumeration_conjunction::newline ) );
    }
}

void map_dig_info::deserialize( JsonIn &jsin )
{
    JsonObject jo = jsin.get_object();

    assign( jo, "digging_min", dig_min );
    assign( jo, "result_ter", result_ter );
    assign( jo, "num_minutes", num_minutes );
    // Support for individual items specified or an itemgroup
    if( jo.has_array( "items" ) ) {
        result_items = item_group::load_item_group( jo.get_member( "items" ), "collection" );
    } else if( jo.has_string( "items" ) ) {
        assign( jo, "items", result_items );
    }
}

map_deconstruct_info::map_deconstruct_info() : can_do( false ), deconstruct_above( false ),
    ter_set( ter_str_id::NULL_ID() ), furn_set( furn_str_id::NULL_ID() ) {}

bool map_deconstruct_info::load( const JsonObject &jsobj, const std::string &member,
                                 bool is_furniture )
{
    if( !jsobj.has_object( member ) ) {
        return false;
    }
    JsonObject j = jsobj.get_object( member );
    furn_set = furn_str_id( j.get_string( "furn_set", "f_null" ) );

    if( !is_furniture ) {
        ter_set = ter_str_id( j.get_string( "ter_set" ) );
    }
    can_do = true;
    deconstruct_above = j.get_bool( "deconstruct_above", false );

    drop_group = item_group::load_item_group( j.get_member( "items" ), "collection" );
    return true;
}

furn_workbench_info::furn_workbench_info() : multiplier( 1.0f ), allowed_mass( units::mass_max ),
    allowed_volume( units::volume_max ) {}

void furn_workbench_info::deserialize( JsonIn &jsin )
{
    JsonObject j = jsin.get_object();

    assign( j, "multiplier", multiplier );
    assign( j, "mass", allowed_mass );
    assign( j, "volume", allowed_volume );
}

plant_data::plant_data() : transform( furn_str_id::NULL_ID() ), base( furn_str_id::NULL_ID() ),
    growth_multiplier( 1.0f ), harvest_multiplier( 1.0f ) {}

void plant_data::deserialize( JsonIn &jsin )
{
    JsonObject j = jsin.get_object();

    assign( j, "transform", transform );
    assign( j, "base", base );
    assign( j, "growth_multiplier", growth_multiplier );
    assign( j, "harvest_multiplier", harvest_multiplier );
}

pry_result::pry_result() : pry_quality( -1 ), pry_bonus_mult( 1 ),
    difficulty( 1 ), noise( 0 ),
    alarm( false ), breakable( false ),
    new_ter_type( ter_str_id::NULL_ID() ), new_furn_type( furn_str_id::NULL_ID() ),
    break_ter_type( ter_str_id::NULL_ID() ), break_furn_type( furn_str_id::NULL_ID() ),
    pry_items( item_group_id( "EMPTY_GROUP" ) ), break_items( item_group_id( "EMPTY_GROUP" ) ) {}

bool pry_result::load( const JsonObject &jsobj, const std::string &member,
                       map_object_type obj_type )
{
    if( !jsobj.has_object( member ) ) {
        return false;
    }

    JsonObject j = jsobj.get_object( member );
    pry_quality = j.get_int( "pry_quality", -1 );
    pry_bonus_mult = j.get_int( "pry_bonus_mult", 1 );
    difficulty = j.get_int( "difficulty", 1 );

    noise = j.get_int( "noise", 0 );
    break_noise = j.get_int( "break_noise", noise );
    sound = to_translation( "crunch!" );
    break_sound = to_translation( "crack!" );
    alarm = j.get_bool( "alarm", false );
    breakable = j.get_bool( "breakable", false );

    switch( obj_type ) {
        case pry_result::furniture:
            new_furn_type = furn_str_id( j.get_string( "new_furn_type", "f_null" ) );
            break_furn_type = furn_str_id( j.get_string( "break_furn_type", "f_null" ) );
            break;
        case pry_result::terrain:
            new_ter_type = ter_str_id( j.get_string( "new_ter_type", "t_null" ) );
            break_ter_type = ter_str_id( j.get_string( "break_ter_type", "t_null" ) );
            break;
    }

    if( j.has_member( "pry_items" ) ) {
        pry_items = item_group::load_item_group( j.get_member( "pry_items" ), "collection" );
    } else {
        pry_items = item_group_id( "EMPTY_GROUP" );
    }

    if( j.has_member( "break_items" ) ) {
        break_items = item_group::load_item_group( j.get_member( "break_items" ), "collection" );
    } else {
        break_items = item_group_id( "EMPTY_GROUP" );
    }

    j.read( "sound", sound );
    j.read( "break_sound", break_sound );

    j.read( "success_message", success_message );
    j.read( "fail_message", fail_message );
    j.read( "break_message", break_message );

    return true;
}

furn_t null_furniture_t()
{
    furn_t new_furniture;
    new_furniture.id = furn_str_id::NULL_ID();
    new_furniture.name_ = translate_marker( "nothing" );
    new_furniture.symbol_.fill( ' ' );
    new_furniture.color_.fill( c_white );
    new_furniture.light_emitted = 0;
    new_furniture.movecost = 0;
    new_furniture.move_str_req = -1;
    new_furniture.transparent = true;
    new_furniture.set_flag( flag_TRANSPARENT );
    new_furniture.examine = iexamine_function_from_string( "none" );
    new_furniture.max_volume = DEFAULT_MAX_VOLUME_IN_SQUARE;
    return new_furniture;
}

ter_t::ter_t() : open( ter_str_id::NULL_ID() ), close( ter_str_id::NULL_ID() ),
    transforms_into( ter_str_id::NULL_ID() ),
    roof( ter_str_id::NULL_ID() ), trap( tr_null ) {}

ter_t null_terrain_t()
{
    ter_t new_terrain;

    new_terrain.id = ter_str_id::NULL_ID();
    new_terrain.name_ = translate_marker( "nothing" );
    new_terrain.symbol_.fill( ' ' );
    new_terrain.color_.fill( c_white );
    new_terrain.light_emitted = 0;
    new_terrain.movecost = 0;
    new_terrain.transparent = true;
    new_terrain.set_flag( flag_TRANSPARENT );
    new_terrain.examine = iexamine_function_from_string( "none" );
    new_terrain.max_volume = DEFAULT_MAX_VOLUME_IN_SQUARE;
    return new_terrain;
}

template<typename C, typename F>
void load_season_array( const JsonObject &jo, const std::string &key, C &container, F load_func )
{
    if( !jo.has_member( key ) ) {
        // Throw 'member not found' error
        jo.get_member( key );

    } else if( jo.has_string( key ) ) {
        container.fill( load_func( jo.get_string( key ) ) );

    } else if( jo.has_array( key ) ) {
        auto arr = jo.get_array( key );
        if( arr.size() == 1 ) {
            container.fill( load_func( arr.get_string( 0 ) ) );

        } else if( arr.size() == container.size() ) {
            for( auto &e : container ) {
                e = load_func( arr.next_string() );
            }

        } else {
            jo.throw_error( "Incorrect number of entries", key );
        }

    } else {
        jo.throw_error( "Expected string or array", key );
    }
}

std::string map_data_common_t::name() const
{
    return _( name_ );
}

void map_data_common_t::load_symbol( const JsonObject &jo )
{
    if( jo.has_member( "copy-from" ) && looks_like.empty() ) {
        looks_like = jo.get_string( "copy-from" );
    }
    jo.read( "looks_like", looks_like );

    load_season_array( jo, "symbol", symbol_, [&jo]( const std::string & str ) {
        if( str == "LINE_XOXO" ) {
            return LINE_XOXO;
        } else if( str == "LINE_OXOX" ) {
            return LINE_OXOX;
        } else if( str.length() != 1 ) {
            jo.throw_error( "Symbol string must be exactly 1 character long.", "symbol" );
        }
        return static_cast<int>( str[0] );
    } );

    const bool has_color = jo.has_member( "color" );
    const bool has_bgcolor = jo.has_member( "bgcolor" );
    if( has_color && has_bgcolor ) {
        jo.throw_error( "Found both color and bgcolor, only one of these is allowed." );
    } else if( has_color ) {
        load_season_array( jo, "color", color_, []( const std::string & str ) {
            // has to use a lambda because of default params
            return color_from_string( str );
        } );
    } else if( has_bgcolor ) {
        load_season_array( jo, "bgcolor", color_, bgcolor_from_string );
    } else {
        jo.throw_error( R"(Missing member: one of: "color", "bgcolor" must exist.)" );
    }
}

int map_data_common_t::symbol() const
{
    return symbol_[season_of_year( calendar::turn )];
}

nc_color map_data_common_t::color() const
{
    return color_[season_of_year( calendar::turn )];
}

const harvest_id &map_data_common_t::get_harvest() const
{
    return harvest_by_season[season_of_year( calendar::turn )];
}

const std::set<std::string> &map_data_common_t::get_harvest_names() const
{
    static const std::set<std::string> null_names = {};
    const harvest_id &hid = get_harvest();
    return hid.is_null() ? null_names : hid->names();
}

void load_furniture( const JsonObject &jo, const std::string &src )
{
    if( furniture_data.empty() ) {
        furniture_data.insert( null_furniture_t() );
    }
    furniture_data.load( jo, src );
}

void load_terrain( const JsonObject &jo, const std::string &src )
{
    if( terrain_data.empty() ) { // TODO: This shouldn't live here
        terrain_data.insert( null_terrain_t() );
    }
    terrain_data.load( jo, src );
}

void map_data_common_t::set_flag( const std::string &flag )
{
    flags.insert( flag );
    const auto it = ter_bitflags_map.find( flag );
    if( it != ter_bitflags_map.end() ) {
        bitflags.set( it->second );
        if( !transparent && it->second == TFLAG_TRANSPARENT ) {
            transparent = true;
        }
        // wall connection check for JSON backwards compatibility
        if( it->second == TFLAG_WALL || it->second == TFLAG_CONNECT_TO_WALL ) {
            set_connects( "WALL" );
        }
    }
}

void map_data_common_t::set_connects( const std::string &connect_group_string )
{
    const auto it = ter_connects_map.find( connect_group_string );
    if( it != ter_connects_map.end() ) {
        connect_group = it->second;
    } else { // arbitrary connect groups are a bad idea for optimization reasons
        debugmsg( "can't find terrain connection group %s", connect_group_string.c_str() );
    }
}

bool map_data_common_t::connects( int &ret ) const
{
    if( connect_group != TERCONN_NONE ) {
        ret = connect_group;
        return true;
    }
    return false;
}

ter_id t_null,
       // Ground
       t_dirt, t_sand, t_clay, t_alluvial_deposit, t_dirtmound, t_pit_shallow, t_pit, t_grave, t_grave_new,
       t_pit_corpsed, t_pit_covered, t_pit_spiked, t_pit_spiked_covered, t_pit_glass, t_pit_glass_covered,
       t_rock_floor,
       t_grass, t_grass_long, t_grass_tall, t_grass_golf, t_grass_dead, t_grass_white, t_moss,
       t_moss_underground, t_metal_floor,
       t_pavement, t_pavement_y, t_sidewalk, t_concrete,
       t_thconc_floor, t_thconc_floor_olight, t_strconc_floor,
       t_floor, t_floor_waxed,
       t_dirtfloor,//Dirt floor(Has roof)
       t_carpet_red, t_carpet_yellow, t_carpet_purple, t_carpet_green,
       t_linoleum_white, t_linoleum_gray,
       t_grate,
       t_slime,
       t_bridge,
       t_covered_well,
       // Lighting related
       t_utility_light,
       // Walls
       t_wall_log_half, t_wall_log, t_wall_log_chipped, t_wall_log_broken, t_palisade, t_palisade_gate,
       t_palisade_gate_o,
       t_wall_half, t_wall_wood, t_wall_wood_chipped, t_wall_wood_broken,
       t_wall, t_concrete_wall, t_brick_wall,
       t_wall_metal,
       t_wall_glass,
       t_wall_glass_alarm,
       t_reinforced_glass, t_reinforced_glass_shutter, t_reinforced_glass_shutter_open,
       t_laminated_glass, t_ballistic_glass,
       t_reinforced_door_glass_o, t_reinforced_door_glass_c,
       t_bars,
       t_reb_cage,
       t_wall_r, t_wall_w, t_wall_b, t_wall_g, t_wall_p, t_wall_y,
       t_door_c, t_door_c_peep, t_door_b, t_door_b_peep, t_door_o, t_door_o_peep, t_rdoor_c, t_rdoor_b,
       t_rdoor_o, t_door_locked_interior, t_door_locked, t_door_locked_peep, t_door_locked_alarm,
       t_door_frame,
       t_chaingate_l, t_fencegate_c, t_fencegate_o, t_chaingate_c, t_chaingate_o,
       t_door_boarded, t_door_boarded_damaged, t_door_boarded_peep, t_rdoor_boarded,
       t_rdoor_boarded_damaged, t_door_boarded_damaged_peep,
       t_door_metal_c, t_door_metal_o, t_door_metal_locked, t_door_metal_pickable, t_mdoor_frame,
       t_door_bar_c, t_door_bar_o, t_door_bar_locked,
       t_door_glass_c, t_door_glass_o, t_door_glass_frosted_c, t_door_glass_frosted_o,
       t_portcullis,
       t_recycler, t_window, t_window_taped, t_window_domestic, t_window_domestic_taped, t_window_open,
       t_curtains, t_window_bars_curtains, t_window_bars_domestic,
       t_window_alarm, t_window_alarm_taped, t_window_empty, t_window_frame, t_window_boarded,
       t_window_boarded_noglass, t_window_reinforced, t_window_reinforced_noglass, t_window_enhanced,
       t_window_enhanced_noglass, t_window_bars_alarm, t_window_bars,
       t_window_stained_green, t_window_stained_red, t_window_stained_blue,
       t_window_no_curtains, t_window_no_curtains_open, t_window_no_curtains_taped,
       t_rock, t_fault,
       t_paper,
       t_rock_wall, t_rock_wall_half,
       // Tree
       t_tree, t_tree_young, t_tree_apple, t_tree_apple_harvested, t_tree_coffee, t_tree_coffee_harvested,
       t_tree_pear, t_tree_pear_harvested, t_tree_cherry, t_tree_cherry_harvested,
       t_tree_peach, t_tree_peach_harvested, t_tree_apricot, t_tree_apricot_harvested, t_tree_plum,
       t_tree_plum_harvested,
       t_tree_pine, t_tree_blackjack, t_tree_birch, t_tree_willow, t_tree_maple, t_tree_maple_tapped,
       t_tree_hickory, t_tree_hickory_dead, t_tree_hickory_harvested, t_tree_deadpine, t_underbrush,
       t_shrub, t_shrub_blueberry, t_shrub_strawberry, t_trunk, t_stump,
       t_root_wall,
       t_wax, t_floor_wax,
       t_fence, t_chainfence, t_chainfence_posts,
       t_fence_post, t_fence_wire, t_fence_barbed, t_fence_rope,
       t_railing,
       // Nether
       t_marloss, t_fungus_floor_in, t_fungus_floor_sup, t_fungus_floor_out, t_fungus_wall,
       t_fungus_mound, t_fungus, t_shrub_fungal, t_tree_fungal, t_tree_fungal_young, t_marloss_tree,
       // Water, lava, etc.
       t_water_moving_dp, t_water_moving_sh, t_water_sh, t_water_dp, t_swater_sh, t_swater_dp,
       t_water_cube, t_lake_bed, t_lake_moss, t_water_pool, t_sewage,
       t_lava,
       // More embellishments than you can shake a stick at.
       t_sandbox, t_slide, t_monkey_bars, t_backboard,
       t_gas_pump, t_gas_pump_smashed,
       t_diesel_pump, t_diesel_pump_smashed,
       t_atm,
       t_generator_broken,
       t_missile, t_missile_exploded,
       t_radio_tower, t_radio_controls,
       t_console_broken, t_console, t_gates_mech_control, t_gates_control_concrete, t_gates_control_brick,
       t_barndoor, t_palisade_pulley,
       t_gates_control_metal,
       t_sewage_pipe, t_sewage_pump,
       t_centrifuge,
       t_column,
       t_vat,
       t_rootcellar,
       t_cvdbody, t_cvdmachine,
       t_nanofab, t_nanofab_body,
       t_water_pump,
       t_conveyor, t_machinery_light, t_machinery_heavy, t_machinery_old, t_machinery_electronic,
       t_improvised_shelter,
       // Staircases etc.
       t_stairs_down, t_stairs_up, t_manhole, t_ladder_up, t_ladder_down, t_slope_down,
       t_slope_up, t_rope_up,
       t_manhole_cover,
       // Special
       t_card_science, t_card_military, t_card_industrial, t_card_reader_broken, t_slot_machine,
       t_elevator_control, t_elevator_control_off, t_elevator, t_pedestal_wyrm,
       t_pedestal_temple,
       // Temple tiles
       t_rock_red, t_rock_green, t_rock_blue, t_floor_red, t_floor_green, t_floor_blue,
       t_switch_rg, t_switch_gb, t_switch_rb, t_switch_even, t_open_air, t_plut_generator,
       t_pavement_bg_dp, t_pavement_y_bg_dp, t_sidewalk_bg_dp, t_guardrail_bg_dp,
       t_rad_platform,
       // Railroad and subway
       t_railroad_rubble,
       t_buffer_stop, t_railroad_crossing_signal, t_crossbuck_wood, t_crossbuck_metal,
       t_railroad_tie, t_railroad_tie_h, t_railroad_tie_v, t_railroad_tie_d,
       t_railroad_track, t_railroad_track_h, t_railroad_track_v, t_railroad_track_d, t_railroad_track_d1,
       t_railroad_track_d2,
       t_railroad_track_on_tie, t_railroad_track_h_on_tie, t_railroad_track_v_on_tie,
       t_railroad_track_d_on_tie;

// TODO: Put this crap into an inclusion, which should be generated automatically using JSON data

void set_ter_ids()
{
    t_null = ter_id( "t_null" );
    t_dirt = ter_id( "t_dirt" );
    t_sand = ter_id( "t_sand" );
    t_clay = ter_id( "t_clay" );
    t_alluvial_deposit = ter_id( "t_alluvial_deposit" );
    t_dirtmound = ter_id( "t_dirtmound" );
    t_grave = ter_id( "t_grave" );
    t_grave_new = ter_id( "t_grave_new" );
    t_pit_shallow = ter_id( "t_pit_shallow" );
    t_pit = ter_id( "t_pit" );
    t_pit_corpsed = ter_id( "t_pit_corpsed" );
    t_pit_covered = ter_id( "t_pit_covered" );
    t_pit_spiked = ter_id( "t_pit_spiked" );
    t_pit_spiked_covered = ter_id( "t_pit_spiked_covered" );
    t_pit_glass = ter_id( "t_pit_glass" );
    t_pit_glass_covered = ter_id( "t_pit_glass_covered" );
    t_rock_floor = ter_id( "t_rock_floor" );
    t_grass = ter_id( "t_grass" );
    t_grass_long = ter_id( "t_grass_long" );
    t_grass_tall = ter_id( "t_grass_tall" );
    t_moss = ter_id( "t_moss" );
    t_moss_underground = ter_id( "t_moss_underground" );
    t_metal_floor = ter_id( "t_metal_floor" );
    t_pavement = ter_id( "t_pavement" );
    t_pavement_y = ter_id( "t_pavement_y" );
    t_sidewalk = ter_id( "t_sidewalk" );
    t_concrete = ter_id( "t_concrete" );
    t_thconc_floor = ter_id( "t_thconc_floor" );
    t_thconc_floor_olight = ter_id( "t_thconc_floor_olight" );
    t_strconc_floor = ter_id( "t_strconc_floor" );
    t_floor = ter_id( "t_floor" );
    t_floor_waxed = ter_id( "t_floor_waxed" );
    t_dirtfloor = ter_id( "t_dirtfloor" );
    t_carpet_red = ter_id( "t_carpet_red" );
    t_carpet_yellow = ter_id( "t_carpet_yellow" );
    t_carpet_purple = ter_id( "t_carpet_purple" );
    t_carpet_green = ter_id( "t_carpet_green" );
    t_linoleum_white = ter_id( "t_linoleum_white" );
    t_linoleum_gray = ter_id( "t_linoleum_gray" );
    t_grate = ter_id( "t_grate" );
    t_slime = ter_id( "t_slime" );
    t_bridge = ter_id( "t_bridge" );
    t_utility_light = ter_id( "t_utility_light" );
    t_wall_log_half = ter_id( "t_wall_log_half" );
    t_wall_log = ter_id( "t_wall_log" );
    t_wall_log_chipped = ter_id( "t_wall_log_chipped" );
    t_wall_log_broken = ter_id( "t_wall_log_broken" );
    t_palisade = ter_id( "t_palisade" );
    t_palisade_gate = ter_id( "t_palisade_gate" );
    t_palisade_gate_o = ter_id( "t_palisade_gate_o" );
    t_wall_half = ter_id( "t_wall_half" );
    t_wall_wood = ter_id( "t_wall_wood" );
    t_wall_wood_chipped = ter_id( "t_wall_wood_chipped" );
    t_wall_wood_broken = ter_id( "t_wall_wood_broken" );
    t_wall = ter_id( "t_wall" );
    t_concrete_wall = ter_id( "t_concrete_wall" );
    t_brick_wall = ter_id( "t_brick_wall" );
    t_wall_metal = ter_id( "t_wall_metal" );
    t_wall_glass = ter_id( "t_wall_glass" );
    t_wall_glass_alarm = ter_id( "t_wall_glass_alarm" );
    t_reinforced_glass = ter_id( "t_reinforced_glass" );
    t_reinforced_glass_shutter = ter_id( "t_reinforced_glass_shutter" );
    t_reinforced_glass_shutter_open = ter_id( "t_reinforced_glass_shutter_open" );
    t_laminated_glass = ter_id( "t_laminated_glass" );
    t_ballistic_glass = ter_id( "t_ballistic_glass" ),
    t_reinforced_door_glass_c = ter_id( "t_reinforced_door_glass_c" );
    t_reinforced_door_glass_o = ter_id( "t_reinforced_door_glass_o" );
    t_bars = ter_id( "t_bars" );
    t_reb_cage = ter_id( "t_reb_cage" );
    t_wall_b = ter_id( "t_wall_b" );
    t_wall_g = ter_id( "t_wall_g" );
    t_wall_p = ter_id( "t_wall_p" );
    t_wall_r = ter_id( "t_wall_r" );
    t_wall_w = ter_id( "t_wall_w" );
    t_door_c = ter_id( "t_door_c" );
    t_door_c_peep = ter_id( "t_door_c_peep" );
    t_door_b = ter_id( "t_door_b" );
    t_door_b_peep = ter_id( "t_door_b_peep" );
    t_door_o = ter_id( "t_door_o" );
    t_door_o_peep = ter_id( "t_door_o_peep" );
    t_rdoor_c = ter_id( "t_rdoor_c" );
    t_rdoor_b = ter_id( "t_rdoor_b" );
    t_rdoor_o = ter_id( "t_rdoor_o" );
    t_door_locked_interior = ter_id( "t_door_locked_interior" );
    t_door_locked = ter_id( "t_door_locked" );
    t_door_locked_peep = ter_id( "t_door_locked_peep" );
    t_door_locked_alarm = ter_id( "t_door_locked_alarm" );
    t_door_frame = ter_id( "t_door_frame" );
    t_mdoor_frame = ter_id( "t_mdoor_frame" );
    t_chaingate_l = ter_id( "t_chaingate_l" );
    t_fencegate_c = ter_id( "t_fencegate_c" );
    t_fencegate_o = ter_id( "t_fencegate_o" );
    t_chaingate_c = ter_id( "t_chaingate_c" );
    t_chaingate_o = ter_id( "t_chaingate_o" );
    t_door_boarded = ter_id( "t_door_boarded" );
    t_door_boarded_damaged = ter_id( "t_door_boarded_damaged" );
    t_door_boarded_peep = ter_id( "t_door_boarded_peep" );
    t_rdoor_boarded = ter_id( "t_rdoor_boarded" );
    t_rdoor_boarded_damaged = ter_id( "t_rdoor_boarded_damaged" );
    t_door_boarded_damaged_peep = ter_id( "t_door_boarded_damaged_peep" );
    t_door_metal_c = ter_id( "t_door_metal_c" );
    t_door_metal_o = ter_id( "t_door_metal_o" );
    t_door_metal_locked = ter_id( "t_door_metal_locked" );
    t_door_metal_pickable = ter_id( "t_door_metal_pickable" );
    t_door_bar_c = ter_id( "t_door_bar_c" );
    t_door_bar_o = ter_id( "t_door_bar_o" );
    t_door_bar_locked = ter_id( "t_door_bar_locked" );
    t_door_glass_c = ter_id( "t_door_glass_c" );
    t_door_glass_o = ter_id( "t_door_glass_o" );
    t_door_glass_frosted_c = ter_id( "t_door_glass_frosted_c" );
    t_door_glass_frosted_o = ter_id( "t_door_glass_frosted_o" );
    t_portcullis = ter_id( "t_portcullis" );
    t_recycler = ter_id( "t_recycler" );
    t_window = ter_id( "t_window" );
    t_window_taped = ter_id( "t_window_taped" );
    t_window_domestic = ter_id( "t_window_domestic" );
    t_window_domestic_taped = ter_id( "t_window_domestic_taped" );
    t_window_bars_domestic = ter_id( "t_window_bars_domestic" );
    t_window_open = ter_id( "t_window_open" );
    t_curtains = ter_id( "t_curtains" );
    t_window_bars_curtains = ter_id( "t_window_bars_curtains" );
    t_window_alarm = ter_id( "t_window_alarm" );
    t_window_alarm_taped = ter_id( "t_window_alarm_taped" );
    t_window_empty = ter_id( "t_window_empty" );
    t_window_frame = ter_id( "t_window_frame" );
    t_window_boarded = ter_id( "t_window_boarded" );
    t_window_boarded_noglass = ter_id( "t_window_boarded_noglass" );
    t_window_reinforced = ter_id( "t_window_reinforced" );
    t_window_reinforced_noglass = ter_id( "t_window_reinforced_noglass" );
    t_window_enhanced = ter_id( "t_window_enhanced" );
    t_window_enhanced_noglass = ter_id( "t_window_enhanced_noglass" );
    t_window_bars_alarm = ter_id( "t_window_bars_alarm" );
    t_window_bars = ter_id( "t_window_bars" );
    t_window_stained_green = ter_id( "t_window_stained_green" );
    t_window_stained_red = ter_id( "t_window_stained_red" );
    t_window_stained_blue = ter_id( "t_window_stained_blue" );
    t_window_no_curtains = ter_id( "t_window_no_curtains" );
    t_window_no_curtains_open = ter_id( "t_window_no_curtains_open" );
    t_window_no_curtains_taped = ter_id( "t_window_no_curtains_taped" );
    t_rock = ter_id( "t_rock" );
    t_fault = ter_id( "t_fault" );
    t_paper = ter_id( "t_paper" );
    t_rock_wall = ter_id( "t_rock_wall" );
    t_rock_wall_half = ter_id( "t_rock_wall_half" );
    t_tree = ter_id( "t_tree" );
    t_tree_young = ter_id( "t_tree_young" );
    t_tree_apple = ter_id( "t_tree_apple" );
    t_tree_apple_harvested = ter_id( "t_tree_apple_harvested" );
    t_tree_coffee = ter_id( "t_tree_coffee" );
    t_tree_coffee_harvested = ter_id( "t_tree_coffee_harvested" );
    t_tree_pear = ter_id( "t_tree_pear" );
    t_tree_pear_harvested = ter_id( "t_tree_pear_harvested" );
    t_tree_cherry = ter_id( "t_tree_cherry" );
    t_tree_cherry_harvested = ter_id( "t_tree_cherry_harvested" );
    t_tree_peach = ter_id( "t_tree_peach" );
    t_tree_peach_harvested = ter_id( "t_tree_peach_harvested" );
    t_tree_apricot = ter_id( "t_tree_apricot" );
    t_tree_apricot_harvested = ter_id( "t_tree_apricot_harvested" );
    t_tree_plum = ter_id( "t_tree_plum" );
    t_tree_plum_harvested = ter_id( "t_tree_plum_harvested" );
    t_tree_pine = ter_id( "t_tree_pine" );
    t_tree_blackjack = ter_id( "t_tree_blackjack" );
    t_tree_birch = ter_id( "t_tree_birch" );
    t_tree_willow = ter_id( "t_tree_willow" );
    t_tree_maple = ter_id( "t_tree_maple" );
    t_tree_maple_tapped = ter_id( "t_tree_maple_tapped" );
    t_tree_deadpine = ter_id( "t_tree_deadpine" );
    t_tree_hickory = ter_id( "t_tree_hickory" );
    t_tree_hickory_dead = ter_id( "t_tree_hickory_dead" );
    t_tree_hickory_harvested = ter_id( "t_tree_hickory_harvested" );
    t_underbrush = ter_id( "t_underbrush" );
    t_shrub = ter_id( "t_shrub" );
    t_shrub_blueberry = ter_id( "t_shrub_blueberry" );
    t_shrub_strawberry = ter_id( "t_shrub_strawberry" );
    t_trunk = ter_id( "t_trunk" );
    t_stump = ter_id( "t_stump" );
    t_root_wall = ter_id( "t_root_wall" );
    t_wax = ter_id( "t_wax" );
    t_floor_wax = ter_id( "t_floor_wax" );
    t_fence = ter_id( "t_fence" );
    t_chainfence = ter_id( "t_chainfence" );
    t_chainfence_posts = ter_id( "t_chainfence_posts" );
    t_fence_post = ter_id( "t_fence_post" );
    t_fence_wire = ter_id( "t_fence_wire" );
    t_fence_barbed = ter_id( "t_fence_barbed" );
    t_fence_rope = ter_id( "t_fence_rope" );
    t_railing = ter_id( "t_railing" );
    t_marloss = ter_id( "t_marloss" );
    t_fungus_floor_in = ter_id( "t_fungus_floor_in" );
    t_fungus_floor_sup = ter_id( "t_fungus_floor_sup" );
    t_fungus_floor_out = ter_id( "t_fungus_floor_out" );
    t_fungus_wall = ter_id( "t_fungus_wall" );
    t_fungus_mound = ter_id( "t_fungus_mound" );
    t_fungus = ter_id( "t_fungus" );
    t_shrub_fungal = ter_id( "t_shrub_fungal" );
    t_tree_fungal = ter_id( "t_tree_fungal" );
    t_tree_fungal_young = ter_id( "t_tree_fungal_young" );
    t_marloss_tree = ter_id( "t_marloss_tree" );
    t_water_moving_dp = ter_id( "t_water_moving_dp" );
    t_water_moving_sh = ter_id( "t_water_moving_sh" );
    t_water_sh = ter_id( "t_water_sh" );
    t_water_dp = ter_id( "t_water_dp" );
    t_swater_sh = ter_id( "t_swater_sh" );
    t_swater_dp = ter_id( "t_swater_dp" );
    t_water_cube = ter_id( "t_water_cube" );
    t_lake_bed  = ter_id( "t_lake_bed" );
    t_lake_moss  = ter_id( "t_lake_moss" );
    t_water_pool = ter_id( "t_water_pool" );
    t_sewage = ter_id( "t_sewage" );
    t_lava = ter_id( "t_lava" );
    t_sandbox = ter_id( "t_sandbox" );
    t_slide = ter_id( "t_slide" );
    t_monkey_bars = ter_id( "t_monkey_bars" );
    t_backboard = ter_id( "t_backboard" );
    t_gas_pump = ter_id( "t_gas_pump" );
    t_gas_pump_smashed = ter_id( "t_gas_pump_smashed" );
    t_diesel_pump = ter_id( "t_diesel_pump" );
    t_diesel_pump_smashed = ter_id( "t_diesel_pump_smashed" );
    t_atm = ter_id( "t_atm" );
    t_generator_broken = ter_id( "t_generator_broken" );
    t_missile = ter_id( "t_missile" );
    t_missile_exploded = ter_id( "t_missile_exploded" );
    t_radio_tower = ter_id( "t_radio_tower" );
    t_radio_controls = ter_id( "t_radio_controls" );
    t_console_broken = ter_id( "t_console_broken" );
    t_console = ter_id( "t_console" );
    t_gates_mech_control = ter_id( "t_gates_mech_control" );
    t_gates_control_brick = ter_id( "t_gates_control_brick" );
    t_gates_control_concrete = ter_id( "t_gates_control_concrete" );
    t_barndoor = ter_id( "t_barndoor" );
    t_palisade_pulley = ter_id( "t_palisade_pulley" );
    t_gates_control_metal = ter_id( "t_gates_control_metal" );
    t_sewage_pipe = ter_id( "t_sewage_pipe" );
    t_sewage_pump = ter_id( "t_sewage_pump" );
    t_centrifuge = ter_id( "t_centrifuge" );
    t_column = ter_id( "t_column" );
    t_vat = ter_id( "t_vat" );
    t_rootcellar = ter_id( "t_rootcellar" );
    t_cvdbody = ter_id( "t_cvdbody" );
    t_cvdmachine = ter_id( "t_cvdmachine" );
    t_nanofab = ter_id( "t_nanofab" );
    t_nanofab_body = ter_id( "t_nanofab_body" );
    t_stairs_down = ter_id( "t_stairs_down" );
    t_stairs_up = ter_id( "t_stairs_up" );
    t_manhole = ter_id( "t_manhole" );
    t_ladder_up = ter_id( "t_ladder_up" );
    t_ladder_down = ter_id( "t_ladder_down" );
    t_slope_down = ter_id( "t_slope_down" );
    t_slope_up = ter_id( "t_slope_up" );
    t_rope_up = ter_id( "t_rope_up" );
    t_manhole_cover = ter_id( "t_manhole_cover" );
    t_card_science = ter_id( "t_card_science" );
    t_card_military = ter_id( "t_card_military" );
    t_card_industrial = ter_id( "t_card_industrial" );
    t_card_reader_broken = ter_id( "t_card_reader_broken" );
    t_slot_machine = ter_id( "t_slot_machine" );
    t_elevator_control = ter_id( "t_elevator_control" );
    t_elevator_control_off = ter_id( "t_elevator_control_off" );
    t_elevator = ter_id( "t_elevator" );
    t_pedestal_wyrm = ter_id( "t_pedestal_wyrm" );
    t_pedestal_temple = ter_id( "t_pedestal_temple" );
    t_rock_red = ter_id( "t_rock_red" );
    t_rock_green = ter_id( "t_rock_green" );
    t_rock_blue = ter_id( "t_rock_blue" );
    t_floor_red = ter_id( "t_floor_red" );
    t_floor_green = ter_id( "t_floor_green" );
    t_floor_blue = ter_id( "t_floor_blue" );
    t_switch_rg = ter_id( "t_switch_rg" );
    t_switch_gb = ter_id( "t_switch_gb" );
    t_switch_rb = ter_id( "t_switch_rb" );
    t_switch_even = ter_id( "t_switch_even" );
    t_covered_well = ter_id( "t_covered_well" );
    t_water_pump = ter_id( "t_water_pump" );
    t_conveyor = ter_id( "t_conveyor" );
    t_machinery_light = ter_id( "t_machinery_light" );
    t_machinery_heavy = ter_id( "t_machinery_heavy" );
    t_machinery_old = ter_id( "t_machinery_old" );
    t_machinery_electronic = ter_id( "t_machinery_electronic" );
    t_open_air = ter_id( "t_open_air" );
    t_plut_generator = ter_id( "t_plut_generator" );
    t_pavement_bg_dp = ter_id( "t_pavement_bg_dp" );
    t_pavement_y_bg_dp = ter_id( "t_pavement_y_bg_dp" );
    t_sidewalk_bg_dp = ter_id( "t_sidewalk_bg_dp" );
    t_guardrail_bg_dp = ter_id( "t_guardrail_bg_dp" );
    t_rad_platform = ter_id( "t_rad_platform" );
    t_improvised_shelter = ter_id( "t_improvised_shelter" );
    t_railroad_rubble = ter_id( "t_railroad_rubble" );
    t_buffer_stop = ter_id( "t_buffer_stop" );
    t_railroad_crossing_signal = ter_id( "t_railroad_crossing_signal" );
    t_crossbuck_metal = ter_id( "t_crossbuck_metal" );
    t_crossbuck_wood = ter_id( "t_crossbuck_wood" );
    t_railroad_tie = ter_id( "t_railroad_tie" );
    t_railroad_tie_h = ter_id( "t_railroad_tie_h" );
    t_railroad_tie_v = ter_id( "t_railroad_tie_v" );
    t_railroad_tie_d = ter_id( "t_railroad_tie_d" );
    t_railroad_track = ter_id( "t_railroad_track" );
    t_railroad_track_h = ter_id( "t_railroad_track_h" );
    t_railroad_track_v = ter_id( "t_railroad_track_v" );
    t_railroad_track_d = ter_id( "t_railroad_track_d" );
    t_railroad_track_d1 = ter_id( "t_railroad_track_d1" );
    t_railroad_track_d2 = ter_id( "t_railroad_track_d2" );
    t_railroad_track_on_tie = ter_id( "t_railroad_track_on_tie" );
    t_railroad_track_h_on_tie = ter_id( "t_railroad_track_h_on_tie" );
    t_railroad_track_v_on_tie = ter_id( "t_railroad_track_v_on_tie" );
    t_railroad_track_d_on_tie = ter_id( "t_railroad_track_d_on_tie" );

    for( auto &elem : terrain_data.get_all() ) {
        ter_t &ter = const_cast<ter_t &>( elem );
        if( ter.trap_id_str.empty() ) {
            ter.trap = tr_null;
        } else {
            ter.trap = trap_str_id( ter.trap_id_str );
        }
    }
}

void reset_furn_ter()
{
    terrain_data.reset();
    furniture_data.reset();
}

furn_id f_null,
        f_hay,
        f_rubble, f_rubble_rock, f_wreckage, f_ash,
        f_barricade_road, f_sandbag_half, f_sandbag_wall,
        f_bulletin,
        f_indoor_plant, f_indoor_plant_y,
        f_bed, f_toilet, f_makeshift_bed, f_straw_bed,
        f_sink, f_oven, f_woodstove, f_fireplace, f_bathtub,
        f_chair, f_armchair, f_sofa, f_cupboard, f_trashcan, f_desk, f_exercise,
        f_ball_mach, f_bench, f_lane, f_table, f_pool_table,
        f_counter,
        f_fridge, f_fridge_on, f_minifreezer_on, f_glass_fridge, f_freezer, f_dresser, f_locker,
        f_rack, f_bookcase,
        f_washer, f_dryer,
        f_vending_c, f_vending_o, f_dumpster, f_dive_block,
        f_crate_c, f_crate_o, f_coffin_c, f_coffin_o,
        f_gunsafe_ml,
        f_large_canvas_wall, f_canvas_wall, f_canvas_door, f_canvas_door_o, f_groundsheet,
        f_fema_groundsheet, f_large_groundsheet,
        f_large_canvas_door, f_large_canvas_door_o, f_center_groundsheet, f_skin_wall, f_skin_door,
        f_skin_door_o, f_skin_groundsheet,
        f_mutpoppy, f_flower_fungal, f_fungal_mass, f_fungal_clump, f_dahlia, f_datura, f_dandelion,
        f_cattails, f_bluebell, f_lake_pondweed, f_lake_detritus, f_lake_liverwort, f_lake_eelgrass,
        f_lake_hornwort, f_cave_mushrooms,
        f_safe_c, f_safe_l, f_safe_o,
        f_plant_seed, f_plant_seedling, f_plant_mature, f_plant_harvest,
        f_fvat_empty, f_fvat_full,
        f_wood_keg,
        f_standing_tank,
        f_statue, f_egg_sackbw, f_egg_sackcs, f_egg_sackws, f_egg_sacke,
        f_flower_marloss,
        f_floor_canvas,
        f_tatami,
        f_kiln_empty, f_kiln_full, f_kiln_metal_empty, f_kiln_metal_full,
        f_arcfurnace_empty, f_arcfurnace_full,
        f_smoking_rack, f_smoking_rack_active, f_metal_smoking_rack, f_metal_smoking_rack_active,
        f_water_mill, f_water_mill_active,
        f_wind_mill, f_wind_mill_active,
        f_robotic_arm, f_vending_reinforced,
        f_brazier,
        f_firering,
        f_tourist_table,
        f_camp_chair,
        f_sign;

void set_furn_ids()
{
    f_null = furn_id( "f_null" );
    f_hay = furn_id( "f_hay" );
    f_rubble = furn_id( "f_rubble" );
    f_rubble_rock = furn_id( "f_rubble_rock" );
    f_wreckage = furn_id( "f_wreckage" );
    f_ash = furn_id( "f_ash" );
    f_barricade_road = furn_id( "f_barricade_road" );
    f_sandbag_half = furn_id( "f_sandbag_half" );
    f_sandbag_wall = furn_id( "f_sandbag_wall" );
    f_bulletin = furn_id( "f_bulletin" );
    f_indoor_plant = furn_id( "f_indoor_plant" );
    f_indoor_plant_y = furn_id( "f_indoor_plant_y" );
    f_bed = furn_id( "f_bed" );
    f_toilet = furn_id( "f_toilet" );
    f_makeshift_bed = furn_id( "f_makeshift_bed" );
    f_straw_bed = furn_id( "f_straw_bed" );
    f_sink = furn_id( "f_sink" );
    f_oven = furn_id( "f_oven" );
    f_woodstove = furn_id( "f_woodstove" );
    f_fireplace = furn_id( "f_fireplace" );
    f_bathtub = furn_id( "f_bathtub" );
    f_chair = furn_id( "f_chair" );
    f_armchair = furn_id( "f_armchair" );
    f_sofa = furn_id( "f_sofa" );
    f_cupboard = furn_id( "f_cupboard" );
    f_trashcan = furn_id( "f_trashcan" );
    f_desk = furn_id( "f_desk" );
    f_exercise = furn_id( "f_exercise" );
    f_ball_mach = furn_id( "f_ball_mach" );
    f_bench = furn_id( "f_bench" );
    f_lane = furn_id( "f_lane" );
    f_table = furn_id( "f_table" );
    f_pool_table = furn_id( "f_pool_table" );
    f_counter = furn_id( "f_counter" );
    f_fridge = furn_id( "f_fridge" );
    f_fridge_on = furn_id( "f_fridge_on" );
    f_minifreezer_on = furn_id( "f_minifreezer_on" );
    f_glass_fridge = furn_id( "f_glass_fridge" );
    f_freezer = furn_id( "f_freezer" );
    f_dresser = furn_id( "f_dresser" );
    f_locker = furn_id( "f_locker" );
    f_rack = furn_id( "f_rack" );
    f_bookcase = furn_id( "f_bookcase" );
    f_washer = furn_id( "f_washer" );
    f_dryer = furn_id( "f_dryer" );
    f_vending_c = furn_id( "f_vending_c" );
    f_vending_o = furn_id( "f_vending_o" );
    f_vending_reinforced = furn_id( "f_vending_reinforced" );
    f_dumpster = furn_id( "f_dumpster" );
    f_dive_block = furn_id( "f_dive_block" );
    f_crate_c = furn_id( "f_crate_c" );
    f_crate_o = furn_id( "f_crate_o" );
    f_coffin_c = furn_id( "f_coffin_c" );
    f_coffin_o = furn_id( "f_coffin_o" );
    f_canvas_wall = furn_id( "f_canvas_wall" );
    f_large_canvas_wall = furn_id( "f_large_canvas_wall" );
    f_canvas_door = furn_id( "f_canvas_door" );
    f_large_canvas_door = furn_id( "f_large_canvas_door" );
    f_canvas_door_o = furn_id( "f_canvas_door_o" );
    f_large_canvas_door_o = furn_id( "f_large_canvas_door_o" );
    f_groundsheet = furn_id( "f_groundsheet" );
    f_large_groundsheet = furn_id( "f_large_groundsheet" );
    f_center_groundsheet = furn_id( "f_center_groundsheet" );
    f_fema_groundsheet = furn_id( "f_fema_groundsheet" );
    f_skin_wall = furn_id( "f_skin_wall" );
    f_skin_door = furn_id( "f_skin_door" );
    f_skin_door_o = furn_id( "f_skin_door_o" );
    f_skin_groundsheet = furn_id( "f_skin_groundsheet" );
    f_mutpoppy = furn_id( "f_mutpoppy" );
    f_fungal_mass = furn_id( "f_fungal_mass" );
    f_fungal_clump = furn_id( "f_fungal_clump" );
    f_flower_fungal = furn_id( "f_flower_fungal" );
    f_bluebell = furn_id( "f_bluebell" );
    f_dahlia = furn_id( "f_dahlia" );
    f_datura = furn_id( "f_datura" );
    f_dandelion = furn_id( "f_dandelion" );
    f_cattails = furn_id( "f_cattails" );
    f_lake_pondweed = furn_id( "f_lake_pondweed" );
    f_lake_detritus = furn_id( "f_lake_detritus" );
    f_lake_liverwort = furn_id( "f_lake_liverwort" );
    f_lake_eelgrass = furn_id( "f_lake_eelgrass" );
    f_lake_hornwort = furn_id( "f_lake_hornwort" );
    f_cave_mushrooms = furn_id( "f_cave_mushrooms" );
    f_safe_c = furn_id( "f_safe_c" );
    f_safe_l = furn_id( "f_safe_l" );
    f_safe_o = furn_id( "f_safe_o" );
    f_plant_seed = furn_id( "f_plant_seed" );
    f_plant_seedling = furn_id( "f_plant_seedling" );
    f_plant_mature = furn_id( "f_plant_mature" );
    f_plant_harvest = furn_id( "f_plant_harvest" );
    f_fvat_empty = furn_id( "f_fvat_empty" );
    f_fvat_full = furn_id( "f_fvat_full" );
    f_wood_keg = furn_id( "f_wood_keg" );
    f_standing_tank = furn_id( "f_standing_tank" );
    f_statue = furn_id( "f_statue" );
    f_egg_sackbw = furn_id( "f_egg_sackbw" );
    f_egg_sackcs = furn_id( "f_egg_sackcs" );
    f_egg_sackws = furn_id( "f_egg_sackws" );
    f_egg_sacke = furn_id( "f_egg_sacke" );
    f_flower_marloss = furn_id( "f_flower_marloss" );
    f_floor_canvas = furn_id( "f_floor_canvas" );
    f_kiln_empty = furn_id( "f_kiln_empty" );
    f_kiln_full = furn_id( "f_kiln_full" );
    f_kiln_metal_empty = furn_id( "f_kiln_metal_empty" );
    f_kiln_metal_full = furn_id( "f_kiln_metal_full" );
    f_arcfurnace_empty = furn_id( "f_arcfurnace_empty" );
    f_arcfurnace_full = furn_id( "f_arcfurnace_full" );
    f_smoking_rack = furn_id( "f_smoking_rack" );
    f_smoking_rack_active = furn_id( "f_smoking_rack_active" );
    f_metal_smoking_rack = furn_id( "f_metal_smoking_rack" );
    f_metal_smoking_rack_active = furn_id( "f_metal_smoking_rack_active" );
    f_water_mill = furn_id( "f_water_mill" );
    f_water_mill_active = furn_id( "f_water_mill_active" );
    f_wind_mill = furn_id( "f_wind_mill" );
    f_wind_mill_active = furn_id( "f_wind_mill_active" );
    f_robotic_arm = furn_id( "f_robotic_arm" );
    f_brazier = furn_id( "f_brazier" );
    f_firering = furn_id( "f_firering" );
    f_tourist_table = furn_id( "f_tourist_table" );
    f_camp_chair = furn_id( "f_camp_chair" );
    f_sign = furn_id( "f_sign" );
    f_gunsafe_ml = furn_id( "f_gunsafe_ml" );
}

size_t ter_t::count()
{
    return terrain_data.size();
}

namespace io
{
template<>
std::string enum_to_string<season_type>( season_type data )
{
    switch( data ) {
        // *INDENT-OFF*
        case season_type::SPRING: return "spring";
        case season_type::SUMMER: return "summer";
        case season_type::AUTUMN: return "autumn";
        case season_type::WINTER: return "winter";
        // *INDENT-ON*
        case season_type::NUM_SEASONS:
            break;
    }
    debugmsg( "Invalid season_type" );
    abort();
}
} // namespace io

void map_data_common_t::load( const JsonObject &jo, const std::string &src )
{
    if( jo.has_member( "examine_action" ) ) {
        examine = iexamine_function_from_string( jo.get_string( "examine_action" ) );
    } else if( !was_loaded ) {
        examine = iexamine_function_from_string( "none" );
    }

    if( jo.has_array( "harvest_by_season" ) ) {
        for( JsonObject harvest_jo : jo.get_array( "harvest_by_season" ) ) {
            auto season_strings = harvest_jo.get_tags( "seasons" );
            std::set<season_type> seasons;
            std::ranges::transform( season_strings, std::inserter( seasons,
                                    seasons.begin() ), io::string_to_enum<season_type> );

            harvest_id hl;
            if( harvest_jo.has_array( "entries" ) ) {
                // TODO: A better inline name - can't use id or name here because it's not set yet
                const size_t num = harvest_list::all().size() + 1;
                hl = harvest_list::load( harvest_jo, src,
                                         string_format( "harvest_inline_%d", static_cast<int>( num ) ) );
            } else if( harvest_jo.has_string( "id" ) ) {
                hl = harvest_id( harvest_jo.get_string( "id" ) );
            } else {
                jo.throw_error( R"(Each harvest entry must specify either "entries" or "id")",
                                "harvest_by_season" );
            }

            for( season_type s : seasons ) {
                harvest_by_season[ s ] = hl;
            }
        }
    }

    mandatory( jo, was_loaded, "description", description );
    optional( jo, was_loaded, "message", message );
    optional( jo, was_loaded, "prompt", prompt );

    assign( jo, "flags", flags );
    bitflags.reset();
    transparent = false;

    for( const std::string &flag : flags ) {
        set_flag( flag );
    }
    optional( jo, was_loaded, "curtain_transform", curtain_transform );
}

bool ter_t::is_null() const
{
    return id == ter_str_id::NULL_ID();
}

bool ter_t::is_diggable() const
{
    return !digging_results.result_ter->is_null();
}

void ter_t::load( const JsonObject &jo, const std::string &src )
{
    connect_group = TERCONN_NONE;
    map_data_common_t::load( jo, src );
    mandatory( jo, was_loaded, "name", name_ );
    mandatory( jo, was_loaded, "move_cost", movecost );
    assign( jo, "coverage", coverage, is_json_check_strict( src ) );
    assign( jo, "max_volume", max_volume, is_json_check_strict( src ) );
    assign( jo, "trap", trap_id_str, is_json_check_strict( src ) );

    assign( jo, "light_emitted", light_emitted, is_json_check_strict( src ) );
    assign( jo, "heat_radiation", heat_radiation, is_json_check_strict( src ) );

    load_symbol( jo );

    trap = tr_null;

    // connect_group is initialized to none, then terrain flags are set, then finally
    // connections from JSON are set. This is so that wall flags can set wall connections
    // but can be overridden by explicit connections in JSON.
    if( jo.has_member( "connects_to" ) ) {
        set_connects( jo.get_string( "connects_to" ) );
    }

    assign( jo, "open", open, is_json_check_strict( src ) );
    assign( jo, "close", close, is_json_check_strict( src ) );
    assign( jo, "transforms_into", transforms_into, is_json_check_strict( src ) );
    assign( jo, "roof", roof, is_json_check_strict( src ) );

    optional( jo, was_loaded, "lockpick_result", lockpick_result, ter_str_id::NULL_ID() );
    optional( jo, was_loaded, "lockpick_message", lockpick_message, translation() );
    optional( jo, was_loaded, "nail_pull_result", nail_pull_result, ter_str_id::NULL_ID() );
    optional( jo, was_loaded, "nail_pull_items", nail_pull_items, {0, 0} );

    oxytorch = cata::make_value<activity_data_ter>();
    if( jo.has_object( "oxytorch" ) ) {
        oxytorch->load( jo.get_object( "oxytorch" ) );
    }

    boltcut = cata::make_value<activity_data_ter>();
    if( jo.has_object( "boltcut" ) ) {
        boltcut->load( jo.get_object( "boltcut" ) );
    }

    hacksaw = cata::make_value<activity_data_ter>();
    if( jo.has_object( "hacksaw" ) ) {
        hacksaw->load( jo.get_object( "hacksaw" ) );
    }

    optional( jo, was_loaded, "fill_result", fill_result, ter_str_id::NULL_ID() );
    optional( jo, was_loaded, "fill_minutes", fill_minutes, 15 );

    // Not assign, because we want to overwrite individual fields
    optional( jo, was_loaded, "bash", bash );
    optional( jo, was_loaded, "digging_results", digging_results );
    deconstruct.load( jo, "deconstruct", false );
    pry.load( jo, "pry", pry_result::terrain );
}

static void check_bash_items( const map_bash_info &mbi, const std::string &id, bool is_terrain )
{
    if( !item_group::group_is_defined( mbi.drop_group ) ) {
        debugmsg( "%s: bash result item group %s does not exist", id.c_str(), mbi.drop_group.c_str() );
    }
    if( mbi.str_max != -1 ) {
        if( is_terrain && mbi.ter_set.is_empty() ) { // Some tiles specify t_null explicitly
            debugmsg( "bash result terrain of %s is undefined/empty", id.c_str() );
        }
        if( !mbi.ter_set.is_valid() ) {
            debugmsg( "bash result terrain %s of %s does not exist", mbi.ter_set.c_str(), id.c_str() );
        }
        if( !mbi.furn_set.is_valid() ) {
            debugmsg( "bash result furniture %s of %s does not exist", mbi.furn_set.c_str(), id.c_str() );
        }
    }

    mbi.check( id, is_terrain ? map_bash_info::map_object_type::terrain :
               map_bash_info::map_object_type::furniture );
}

static void check_decon_items( const map_deconstruct_info &mbi, const std::string &id,
                               bool is_terrain )
{
    if( !mbi.can_do ) {
        return;
    }
    if( !item_group::group_is_defined( mbi.drop_group ) ) {
        debugmsg( "%s: deconstruct result item group %s does not exist", id.c_str(),
                  mbi.drop_group.c_str() );
    }
    if( is_terrain && mbi.ter_set.is_empty() ) { // Some tiles specify t_null explicitly
        debugmsg( "deconstruct result terrain of %s is undefined/empty", id.c_str() );
    }
    if( !mbi.ter_set.is_valid() ) {
        debugmsg( "deconstruct result terrain %s of %s does not exist", mbi.ter_set.c_str(), id.c_str() );
    }
    if( !mbi.furn_set.is_valid() ) {
        debugmsg( "deconstruct result furniture %s of %s does not exist", mbi.furn_set.c_str(),
                  id.c_str() );
    }
}

static void check_pry_items( const pry_result &pry, const std::string &id,
                             bool is_terrain )
{
    if( pry.pry_quality == -1 ) {
        return;
    }
    if( !item_group::group_is_defined( pry.break_items ) ) {
        debugmsg( "%s: pry breakage result item group %s does not exist", id.c_str(),
                  pry.break_items.c_str() );
    }
    if( is_terrain ) {
        if( pry.new_ter_type.is_empty() ) {  // Some tiles specify t_null explicitly
            debugmsg( "pry result terrain of %s is undefined/empty", id.c_str() );
        }
        if( pry.breakable && pry.break_ter_type.is_empty() ) {
            debugmsg( "pry breakage result terrain %s of %s is undefined/empty", id.c_str() );
        }
        if( !pry.new_ter_type.is_valid() ) {  // Some tiles specify t_null explicitly
            debugmsg( "pry result terrain of %s does not exist", pry.new_ter_type.c_str(), id.c_str() );
        }
        if( pry.breakable && !pry.break_ter_type.is_valid() ) {
            debugmsg( "pry breakage result terrain %s of %s does not exist", pry.new_ter_type.c_str(),
                      id.c_str() );
        }
    } else {
        if( pry.new_furn_type.is_empty() ) { // Some tiles specify t_null explicitly
            debugmsg( "pry result furniture of %s is undefined/empty", id.c_str() );
        }
        if( pry.breakable && pry.break_furn_type.is_empty() ) {
            debugmsg( "pry breakage result furniture %s of %s is undefined/empty", id.c_str() );
        }
        if( !pry.new_furn_type.is_valid() ) { // Some tiles specify t_null explicitly
            debugmsg( "pry result furniture of %s does not exist", pry.new_furn_type.c_str(), id.c_str() );
        }
        if( pry.breakable && !pry.break_furn_type.is_valid() ) {
            debugmsg( "pry breakage result furniture %s of %s does not exist", pry.new_furn_type.c_str(),
                      id.c_str() );
        }
    }
}

void ter_t::check() const
{
    map_data_common_t::check();
    check_bash_items( bash, id.str(), true );
    check_decon_items( deconstruct, id.str(), true );
    check_pry_items( pry, id.str(), true );

    if( !transforms_into.is_valid() ) {
        debugmsg( "invalid transforms_into %s for %s", transforms_into.c_str(), id.c_str() );
    }
    if( !open.is_valid() ) {
        debugmsg( "invalid terrain %s for opening %s", open.c_str(), id.c_str() );
    }
    if( id && open == id ) {
        debugmsg( "%s has \"open\" set to itself", id.c_str() );
    }
    if( !close.is_valid() ) {
        debugmsg( "invalid terrain %s for closing %s", close.c_str(), id.c_str() );
    }
    if( id && close == id ) {
        debugmsg( "%s has \"close\" set to itself", id.c_str() );
    }
    if( transforms_into && transforms_into == id ) {
        debugmsg( "%s transforms_into itself", id.c_str() );
    }
    if( bash.ter_set && bash.ter_set == id ) {
        debugmsg( "%s turns into itself when bashed", id.c_str() );
    }
    if( bash.ter_set_bashed_from_above && bash.ter_set_bashed_from_above == id ) {
        debugmsg( "%s turns into itself when bashed from above", id.c_str() );
    }
    if( json_report_strict
        && ( bash.ter_set == t_open_air.id() || bash.ter_set_bashed_from_above == t_open_air.id() ) ) {
        debugmsg( "%s explicitly turns into \"t_open_air\", but \"t_null\" is preferred",
                  id.c_str() );
    }
    if( roof && roof->roof ) {
        debugmsg( "%s has roof %s, which has its own roof %s",
                  id.str(), roof.str(), roof->roof.str() );
    }
    if( roof && !roof->bash.bash_below ) {
        debugmsg( "%s has roof %s, with \"bash_below\": false",
                  id.str(), roof.str() );
    }
    if( bash.ter_set_bashed_from_above && bash.ter_set_bashed_from_above->movecost == 0 &&
        !bash.ter_set_bashed_from_above->roof ) {
        debugmsg( "%s has bash.ter_set_bashed_from_above %s, which is unpassable but has no roof",
                  id.str(), bash.ter_set_bashed_from_above.str() );
    }
    if( json_report_strict && deconstruct.ter_set == t_open_air.id() ) {
        debugmsg( "%s deconstructs into \"t_open_air\", but \"t_null\" is preferred",
                  id.str() );
    }
    if( movecost == 1 || movecost < 0 ) {
        debugmsg( "%s has move_cost %d, but allowed values for terrain are >=2 and 0", id, movecost );
    }
}

const std::vector<ter_t> &ter_t::get_all()
{
    return terrain_data.get_all();
}

furn_t::furn_t() : open( furn_str_id::NULL_ID() ), close( furn_str_id::NULL_ID() ) {}

size_t furn_t::count()
{
    return furniture_data.size();
}

bool furn_t::is_movable() const
{
    return move_str_req >= 0;
}

void furn_t::load( const JsonObject &jo, const std::string &src )
{
    map_data_common_t::load( jo, src );
    mandatory( jo, was_loaded, "name", name_ );
    mandatory( jo, was_loaded, "move_cost_mod", movecost );
    optional( jo, was_loaded, "coverage", coverage );
    optional( jo, was_loaded, "comfort", comfort, 0 );
    optional( jo, was_loaded, "floor_bedding_warmth", floor_bedding_warmth, 0 );
    optional( jo, was_loaded, "emissions", emissions );
    optional( jo, was_loaded, "provides_liquids", provides_liquids );
    optional( jo, was_loaded, "bonus_fire_warmth_feet", bonus_fire_warmth_feet, 300 );
    optional( jo, was_loaded, "keg_capacity", keg_capacity, legacy_volume_reader, 0_ml );
    mandatory( jo, was_loaded, "required_str", move_str_req );
    optional( jo, was_loaded, "max_volume", max_volume, volume_reader(), DEFAULT_MAX_VOLUME_IN_SQUARE );
    optional( jo, was_loaded, "deployed_item", deployed_item );
    load_symbol( jo );

    optional( jo, was_loaded, "light_emitted", light_emitted );

    // see the comment in ter_id::load for connect_group handling
    connect_group = TERCONN_NONE;
    if( jo.has_member( "connects_to" ) ) {
        set_connects( jo.get_string( "connects_to" ) );
    }

    optional( jo, was_loaded, "open", open, furn_str_id::NULL_ID() );
    optional( jo, was_loaded, "close", close, furn_str_id::NULL_ID() );

    optional( jo, was_loaded, "lockpick_result", lockpick_result, furn_str_id::NULL_ID() );
    optional( jo, was_loaded, "lockpick_message", lockpick_message, translation() );


    oxytorch = cata::make_value<activity_data_furn>();
    if( jo.has_object( "oxytorch" ) ) {
        oxytorch->load( jo.get_object( "oxytorch" ) );
    }

    boltcut = cata::make_value<activity_data_furn>();
    if( jo.has_object( "boltcut" ) ) {
        boltcut->load( jo.get_object( "boltcut" ) );
    }

    hacksaw = cata::make_value<activity_data_furn>();
    if( jo.has_object( "hacksaw" ) ) {
        hacksaw->load( jo.get_object( "hacksaw" ) );
    }

    optional( jo, was_loaded, "transforms_into", transforms_into, furn_str_id::NULL_ID() );

    optional( jo, was_loaded, "bash", bash );
    deconstruct.load( jo, "deconstruct", true );
    pry.load( jo, "pry", pry_result::furniture );

    optional( jo, was_loaded, "workbench", workbench );
    optional( jo, was_loaded, "plant_data", plant );
    assign( jo, "surgery_skill_multiplier", surgery_skill_multiplier );

    if( jo.has_member( "active" ) ) {
        JsonIn &jsin = *jo.get_raw( "active" );
        active.deserialize( jsin );
    }

    assign( jo, "crafting_pseudo_item", crafting_pseudo_items );

}

void map_data_common_t::check() const
{
    for( auto &harvest : harvest_by_season ) {
        if( !harvest.is_null() && examine == iexamine::none ) {
            debugmsg( "Harvest data defined without examine function for %s", name_.c_str() );
        }
    }
}

void furn_t::check() const
{
    map_data_common_t::check();
    check_bash_items( bash, id.str(), false );
    check_decon_items( deconstruct, id.str(), false );
    check_pry_items( pry, id.str(), false );

    if( !open.is_valid() ) {
        debugmsg( "invalid furniture %s for opening %s", open.c_str(), id.c_str() );
    }
    if( !close.is_valid() ) {
        debugmsg( "invalid furniture %s for closing %s", close.c_str(), id.c_str() );
    }
    if( has_flag( "EMITTER" ) ) {
        if( emissions.empty() ) {
            debugmsg( "furn %s has the EMITTER flag, but no emissions were set", id.c_str() );
        } else {
            for( const emit_id &e : emissions ) {
                if( !e.is_valid() ) {
                    debugmsg( "furn %s has the EMITTER flag, but invalid emission %s was set", id.c_str(),
                              e.str().c_str() );
                }
            }
        }
    }
}

const std::vector<furn_t> &furn_t::get_all()
{
    return furniture_data.get_all();
}

void finalize_furn()
{
    set_furn_ids();
    // Legacy
    for( const furn_t &furn : furniture_data.get_all() ) {
        if( furn.examine == iexamine::workbench ) {
            furn_t &furn_mutable = const_cast<furn_t &>( furn );
            if( furn_mutable.deployed_item.is_valid() ) {
                furn_mutable.examine = iexamine::deployed_furniture;
            } else {
                furn_mutable.examine = iexamine::none;
            }
        }
    }

}

int activity_byproduct::roll() const
{
    return count + rng( random_min, random_max );
}

void activity_byproduct::load( const JsonObject &jo )
{
    mandatory( jo, was_loaded, "item", item );

    if( jo.has_int( "count" ) ) {
        mandatory( jo, was_loaded, "count", count );
        count = std::max( 0, count );
    } else if( jo.has_array( "count" ) ) {
        std::vector<int> count_random = jo.get_int_array( "count" );
        random_min = std::max( 0, count_random[0] );
        random_max = std::max( 0, count_random[1] );
        if( random_min > random_max ) {
            std::swap( random_min, random_max );
        }
    }
}

void activity_data_common::load( const JsonObject &jo )
{
    optional( jo, was_loaded, "duration", duration_, 1_seconds );
    optional( jo, was_loaded, "message", message_ );
    optional( jo, was_loaded, "sound", sound_ );

    if( jo.has_array( "byproducts" ) ) {
        std::vector<activity_byproduct> entries;
        for( JsonObject activity_entry : jo.get_array( "byproducts" ) ) {
            struct activity_byproduct entry {};
            entry.load( activity_entry );
            byproducts_.push_back( entry );
        }
    }
}

void activity_data_ter::load( const JsonObject &jo )
{
    mandatory( jo, was_loaded, "result", result_ );
    activity_data_common::load( jo );
    valid_ = true;
}

void activity_data_furn::load( const JsonObject &jo )
{
    optional( jo, was_loaded, "result", result_, f_null.id() );
    activity_data_common::load( jo );
    valid_ = true;
}

void check_furniture_and_terrain()
{
    terrain_data.check();
    furniture_data.check();
}
