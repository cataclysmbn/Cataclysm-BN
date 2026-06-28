#include "regional_settings.h"

#include <algorithm>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

#include "all_enum_values.h"
#include "consistency_report.h"
#include "debug.h"
#include "enum_conversions.h"
#include "int_id.h"
#include "json.h"
#include "map_extras.h"
#include "options.h"
#include "overmap_special.h"
#include "rng.h"
#include "string_formatter.h"
#include "string_id.h"
#include "translations.h"

ter_furn_id::ter_furn_id() : ter( t_null ), furn( f_null ) { }

template<typename T>
void read_and_set_or_throw( const JsonObject &jo, const std::string &member, T &target,
                            bool required )
{
    T tmp;
    if( !jo.read( member, tmp ) ) {
        if( required ) {
            jo.throw_error( string_format( "%s required", member ) );
        }
    } else {
        target = tmp;
    }
}

namespace
{

struct compat_map_extra_collection {
    std::optional<std::string> copy_from;
    unsigned int chance = 0;
    weighted_int_list<std::string> values;
};

struct compat_region_settings_map_extras {
    std::optional<std::string> copy_from;
    std::vector<std::string> extras;
};

struct compat_region_terrain_furniture {
    std::optional<std::string> copy_from;
    std::optional<std::string> replaced_terrain_id;
    std::optional<std::string> replaced_furniture_id;
    weighted_int_list<ter_id> terrain;
    weighted_int_list<furn_id> furniture;
};

struct compat_region_settings_terrain_furniture {
    std::optional<std::string> copy_from;
    std::vector<std::string> ter_furn;
};

struct compat_forest_biome_component {
    std::optional<std::string> copy_from;
    forest_biome_component component;
};

struct compat_forest_biome_mapgen {
    std::optional<std::string> copy_from;
    std::vector<std::string> terrains;
    std::vector<std::string> components;
    forest_biome biome;
};

struct compat_region_settings_forest_mapgen {
    std::optional<std::string> copy_from;
    std::vector<std::string> biomes;
};

struct compat_region_settings_city {
    std::optional<std::string> copy_from;
    city_settings city;
};

struct compat_region_settings_forest {
    std::optional<std::string> copy_from;
    overmap_forest_settings forest;
};

struct compat_region_references {
    std::optional<std::string> cities;
    std::optional<std::string> weather;
    std::optional<std::string> map_extras;
    std::optional<std::string> forest_composition;
    std::optional<std::string> forests;
    std::optional<std::string> terrain_furniture;
};

auto compat_weather_generators = std::unordered_map<std::string, weather_generator> {};
auto compat_map_extra_collection_defs =
std::unordered_map<std::string, compat_map_extra_collection> {};
auto compat_region_settings_map_extras_defs = std::unordered_map<std::string,
compat_region_settings_map_extras> {};
auto compat_region_terrain_furniture_defs = std::unordered_map<std::string,
compat_region_terrain_furniture> {};
auto compat_region_settings_terrain_furniture_defs = std::unordered_map<std::string,
compat_region_settings_terrain_furniture> {};
auto compat_forest_biome_component_defs = std::unordered_map<std::string,
compat_forest_biome_component> {};
auto compat_forest_biome_mapgen_defs = std::unordered_map<std::string,
compat_forest_biome_mapgen> {};
auto compat_region_settings_forest_mapgen_defs = std::unordered_map<std::string,
compat_region_settings_forest_mapgen> {};
auto compat_region_settings_city_defs = std::unordered_map<std::string,
compat_region_settings_city> {};
auto compat_region_settings_forest_defs = std::unordered_map<std::string,
compat_region_settings_forest> {};
auto compat_region_refs = std::unordered_map<std::string, compat_region_references> {};

auto get_default_region_settings() -> const regional_settings *
{
    const auto iter = region_settings_map.find( "default" );
    return iter == region_settings_map.end() ? nullptr : &iter->second;
}

auto get_default_weather_generator() -> weather_generator
{
    if( const auto *default_region = get_default_region_settings() ) {
        return default_region->weather;
    }
    return weather_generator {};
}

auto get_region_settings( const std::string &id ) -> const regional_settings *
{
    const auto iter = region_settings_map.find( id );
    return iter == region_settings_map.end() ? nullptr : &iter->second;
}

auto get_default_forest_biome( const std::string &terrain ) -> std::optional<forest_biome>
{
    const auto *default_region = get_default_region_settings();
    if( default_region == nullptr ) {
        return std::nullopt;
    }
    const auto biome_iter = default_region->forest_composition.unfinalized_biomes.find( terrain );
    if( biome_iter == default_region->forest_composition.unfinalized_biomes.end() ) {
        return std::nullopt;
    }
    return biome_iter->second;
}

auto get_default_forest_component( const std::string &copy_from ) ->
std::optional<forest_biome_component>
{
    const auto biome = get_default_forest_biome( "forest" );
    if( !biome ) {
        return std::nullopt;
    }
    const auto component_name = copy_from == "trees_forest" ? "trees" :
                                copy_from == "shrubs_and_flowers_forest" ? "shrubs_and_flowers" :
                                copy_from == "clutter_forest" ? "clutter" : copy_from;
    const auto component_iter = biome->unfinalized_biome_components.find( component_name );
    if( component_iter == biome->unfinalized_biome_components.end() ) {
        return std::nullopt;
    }
    return component_iter->second;
}

auto get_region_city_settings( const std::string &id ) -> std::optional<city_settings>
{
    if( const auto *region = get_region_settings( id ) ) {
        return region->city_spec;
    }
    return std::nullopt;
}

auto get_region_forest_settings( const std::string &id ) -> std::optional<overmap_forest_settings>
{
    if( const auto *region = get_region_settings( id ) ) {
        return region->overmap_forest;
    }
    return std::nullopt;
}

auto consume_bool_if_present( const JsonObject &jo, const std::string &member ) -> void
{
    if( jo.has_bool( member ) ) {
        ( void )jo.get_bool( member );
    }
}

auto disable_lakes( regional_settings &region ) -> void
{
    region.overmap_lake.noise_threshold_lake = 0.0;
}

auto disable_forests( regional_settings &region ) -> void
{
    region.overmap_forest.noise_threshold_forest = 0.0;
    region.overmap_forest.noise_threshold_forest_thick = 0.0;
}

auto make_legacy_default_oter( const oter_str_id &surface_oter ) ->
std::array<oter_str_id, OVERMAP_LAYERS>
{
    auto result = std::array<oter_str_id, OVERMAP_LAYERS> {};
    const auto open_air = oter_str_id( "open_air" );
    const auto empty_rock = oter_str_id( "empty_rock" );
    for( int z = -OVERMAP_DEPTH; z <= OVERMAP_HEIGHT; ++z ) {
        result[OVERMAP_DEPTH + z] = z == 0 ? surface_oter : ( z > 0 ? open_air : empty_rock );
    }
    return result;
}

auto read_default_oter( const JsonObject &jo,
                        std::array<oter_str_id, OVERMAP_LAYERS> &default_oter ) -> bool
{
    if( !jo.has_member( "default_oter" ) ) {
        return false;
    }

    if( jo.has_array( "default_oter" ) ) {
        auto default_oter_array = jo.get_array( "default_oter" );
        if( default_oter_array.size() != OVERMAP_LAYERS ) {
            jo.throw_error( string_format( "default_oter must contain %d entries", OVERMAP_LAYERS ) );
        }
        for( int i = 0; i < OVERMAP_LAYERS; ++i ) {
            default_oter[i] = oter_str_id( default_oter_array.get_string( i ) );
        }
        std::ranges::reverse( default_oter );
        return true;
    }

    auto surface_oter = oter_str_id();
    if( jo.read( "default_oter", surface_oter ) ) {
        default_oter = make_legacy_default_oter( surface_oter );
        return true;
    }

    return false;
}

auto read_unfinalized_weighted_strings( const JsonObject &jo, const std::string &member,
                                        std::map<std::string, int> &values ) -> void
{
    if( !jo.has_member( member ) ) {
        return;
    }
    values.clear();
    if( jo.has_object( member ) ) {
        for( const JsonMember entry : jo.get_object( member ) ) {
            if( entry.is_comment() ) {
                continue;
            }
            values[entry.name()] = entry.get_int();
        }
        return;
    }
    for( const JsonValue entry : jo.get_array( member ) ) {
        if( entry.test_string() ) {
            values[entry.get_string()] = 100;
        } else if( entry.test_array() ) {
            auto pair = entry.get_array();
            values[pair.get_string( 0 )] = pair.get_int( 1 );
        } else {
            entry.throw_error( "expected string or [ id, weight ] pair" );
        }
    }
}

auto append_unfinalized_weighted_strings( const JsonObject &jo, const std::string &member,
        std::map<std::string, int> &values ) -> void
{
    if( !jo.has_member( member ) ) {
        return;
    }
    if( jo.has_object( member ) ) {
        for( const JsonMember entry : jo.get_object( member ) ) {
            if( entry.is_comment() ) {
                continue;
            }
            values[entry.name()] = entry.get_int();
        }
        return;
    }
    for( const JsonValue entry : jo.get_array( member ) ) {
        if( entry.test_string() ) {
            values[entry.get_string()] = 100;
        } else if( entry.test_array() ) {
            auto pair = entry.get_array();
            values[pair.get_string( 0 )] = pair.get_int( 1 );
        } else {
            entry.throw_error( "expected string or [ id, weight ] pair" );
        }
    }
}

auto read_string_list( const JsonObject &jo, const std::string &member,
                       std::vector<std::string> &values, const bool replace ) -> void
{
    if( !jo.has_array( member ) ) {
        return;
    }
    if( replace ) {
        values.clear();
    }
    for( const auto value : jo.get_tags<std::string>( member ) ) {
        values.emplace_back( value );
    }
}

auto read_weighted_string_array( const JsonObject &jo, const std::string &member,
                                 weighted_int_list<std::string> &values,
                                 const bool replace ) -> void
{
    if( !jo.has_array( member ) ) {
        return;
    }
    if( replace ) {
        values = weighted_int_list<std::string> {};
    }
    for( JsonArray entry : jo.get_array( member ) ) {
        values.add_or_replace( entry.get_string( 0 ), entry.get_int( 1 ) );
    }
}

auto read_city_buildings( const JsonObject &jo, const std::string &member,
                          building_bin &dest ) -> void
{
    if( !jo.has_array( member ) ) {
        return;
    }
    for( JsonArray entry : jo.get_array( member ) ) {
        dest.add( overmap_special_id( entry.get_string( 0 ) ), entry.get_int( 1 ) );
    }
}

auto read_compat_forest_terrain_furniture( const JsonObject &jo, forest_biome &biome ) -> void
{
    if( !jo.has_object( "terrain_furniture" ) ) {
        return;
    }
    biome.unfinalized_terrain_dependent_furniture.clear();
    for( const JsonMember entry : jo.get_object( "terrain_furniture" ) ) {
        if( entry.is_comment() ) {
            continue;
        }
        auto terrain_furniture = entry.get_object();
        auto &result = biome.unfinalized_terrain_dependent_furniture[entry.name()];
        terrain_furniture.read( "chance", result.chance );
        read_unfinalized_weighted_strings( terrain_furniture, "furniture", result.unfinalized_furniture );
    }
}

auto resolve_compat_forest_biome( const compat_forest_biome_mapgen &compat_biome ) -> forest_biome
{
    auto result = compat_biome.biome;
    for( const auto &component_id : compat_biome.components ) {
        const auto iter = compat_forest_biome_component_defs.find( component_id );
        if( iter == compat_forest_biome_component_defs.end() ) {
            throw std::runtime_error( string_format( "unknown forest_biome_feature '%s'", component_id ) );
        }
        result.unfinalized_biome_components[component_id] = iter->second.component;
    }
    return result;
}

template<typename T>
auto read_compat_copy_from( const JsonObject &jo, const std::unordered_map<std::string, T> &defs,
                            T &result, const std::string &type_name ) -> void
{
    if( !jo.has_string( "copy-from" ) ) {
        return;
    }
    const auto copy_from = jo.get_string( "copy-from" );
    const auto iter = defs.find( copy_from );
    if( iter == defs.end() ) {
        jo.throw_error( string_format( "unknown %s copy-from '%s'", type_name, copy_from ), "copy-from" );
    }
    result = iter->second;
    result.copy_from = copy_from;
}

} // namespace

auto load_compat_weather_generator( const JsonObject &jo ) -> void
{
    const auto id = jo.get_string( "id" );
    auto result = get_default_weather_generator();
    if( jo.has_string( "copy-from" ) ) {
        const auto copy_from = jo.get_string( "copy-from" );
        if( const auto iter = compat_weather_generators.find( copy_from );
            iter != compat_weather_generators.end() ) {
            result = iter->second;
        } else if( const auto *copy_region = get_region_settings( copy_from ) ) {
            result = copy_region->weather;
        } else {
            jo.throw_error( string_format( "unknown weather_generator copy-from '%s'", copy_from ),
                            "copy-from" );
        }
    }
    if( jo.has_number( "base_temperature" ) ) {
        const auto base_temp = jo.get_float( "base_temperature" );
        for( auto &season : result.season_stats ) {
            season.average_temperature = units::from_celsius( base_temp );
        }
    }
    jo.read( "base_humidity", result.base_humidity );
    jo.read( "base_pressure", result.base_pressure );
    jo.read( "base_wind", result.base_wind );
    jo.read( "base_wind_distrib_peaks", result.base_wind_distrib_peaks );
    jo.read( "base_wind_season_variation", result.base_wind_season_variation );
    if( jo.has_array( "weather_types" ) ) {
        result.weather_types.clear();
        jo.read( "weather_types", result.weather_types );
    }
    if( result.weather_types.empty() ) {
        jo.throw_error( "weather_generator requires weather_types or a copy-from/default weather with weather_types" );
    }
    if( jo.has_array( "weather_black_list" ) ) {
        ( void )jo.get_array( "weather_black_list" );
    }
    compat_weather_generators[id] = result;
}

auto load_compat_forest_biome_feature( const JsonObject &jo ) -> void
{
    const auto id = jo.get_string( "id" );
    auto result = compat_forest_biome_component {};
    if( jo.has_string( "copy-from" ) ) {
        const auto copy_from = jo.get_string( "copy-from" );
        if( const auto iter = compat_forest_biome_component_defs.find( copy_from );
            iter != compat_forest_biome_component_defs.end() ) {
            result = iter->second;
        } else if( const auto component = get_default_forest_component( copy_from ) ) {
            result.component = *component;
        } else {
            jo.throw_error( string_format( "unknown forest_biome_feature copy-from '%s'", copy_from ),
                            "copy-from" );
        }
        result.copy_from = copy_from;
    }
    jo.read( "chance", result.component.chance );
    jo.read( "sequence", result.component.sequence );
    read_unfinalized_weighted_strings( jo, "types", result.component.unfinalized_types );
    if( jo.has_object( "extend" ) ) {
        append_unfinalized_weighted_strings( jo.get_object( "extend" ), "types",
                                             result.component.unfinalized_types );
    }
    compat_forest_biome_component_defs[id] = result;
}

auto load_compat_forest_biome_mapgen( const JsonObject &jo ) -> void
{
    const auto id = jo.get_string( "id" );
    auto result = compat_forest_biome_mapgen {};
    if( jo.has_string( "copy-from" ) ) {
        const auto copy_from = jo.get_string( "copy-from" );
        if( const auto iter = compat_forest_biome_mapgen_defs.find( copy_from );
            iter != compat_forest_biome_mapgen_defs.end() ) {
            result = iter->second;
        } else if( copy_from == "biome_forest_default" ) {
            if( const auto biome = get_default_forest_biome( "forest" ) ) {
                result.biome = *biome;
                result.terrains = { "forest" };
            } else {
                jo.throw_error( "unknown forest_biome_mapgen copy-from 'biome_forest_default'",
                                "copy-from" );
            }
        } else {
            jo.throw_error( string_format( "unknown forest_biome_mapgen copy-from '%s'", copy_from ),
                            "copy-from" );
        }
        result.copy_from = copy_from;
    }
    read_string_list( jo, "terrains", result.terrains, true );
    if( jo.has_member( "components" ) ) {
        result.biome.unfinalized_biome_components.clear();
    }
    read_string_list( jo, "components", result.components, true );
    jo.read( "sparseness_adjacency_factor", result.biome.sparseness_adjacency_factor );
    jo.read( "item_group", result.biome.item_group );
    jo.read( "item_group_chance", result.biome.item_group_chance );
    jo.read( "item_spawn_iterations", result.biome.item_spawn_iterations );
    read_unfinalized_weighted_strings( jo, "groundcover", result.biome.unfinalized_groundcover );
    read_compat_forest_terrain_furniture( jo, result.biome );
    if( jo.has_object( "extend" ) ) {
        const auto extend = jo.get_object( "extend" );
        read_string_list( extend, "terrains", result.terrains, false );
        read_string_list( extend, "components", result.components, false );
    }
    compat_forest_biome_mapgen_defs[id] = result;
}

auto load_compat_map_extra_collection( const JsonObject &jo ) -> void
{
    const auto id = jo.get_string( "id" );
    auto result = compat_map_extra_collection {};
    if( jo.has_string( "copy-from" ) ) {
        const auto copy_from = jo.get_string( "copy-from" );
        if( const auto iter = compat_map_extra_collection_defs.find( copy_from );
            iter != compat_map_extra_collection_defs.end() ) {
            result = iter->second;
        } else if( const auto *default_region = get_default_region_settings(); default_region != nullptr ) {
            const auto extra_iter = default_region->region_extras.find( copy_from );
            if( extra_iter == default_region->region_extras.end() ) {
                jo.throw_error( string_format( "unknown map_extra_collection copy-from '%s'", copy_from ),
                                "copy-from" );
            }
            result.chance = extra_iter->second.chance;
            result.values = extra_iter->second.values;
        } else {
            jo.throw_error( string_format( "unknown map_extra_collection copy-from '%s'", copy_from ),
                            "copy-from" );
        }
        result.copy_from = copy_from;
    }
    jo.read( "chance", result.chance );
    read_weighted_string_array( jo, "extras", result.values, true );
    if( jo.has_object( "extend" ) ) {
        read_weighted_string_array( jo.get_object( "extend" ), "extras", result.values, false );
    }
    compat_map_extra_collection_defs[id] = result;
}

auto load_compat_region_settings_map_extras( const JsonObject &jo ) -> void
{
    const auto id = jo.get_string( "id" );
    auto result = compat_region_settings_map_extras {};
    if( jo.has_string( "copy-from" ) ) {
        const auto copy_from = jo.get_string( "copy-from" );
        if( const auto iter = compat_region_settings_map_extras_defs.find( copy_from );
            iter != compat_region_settings_map_extras_defs.end() ) {
            result = iter->second;
        } else if( const auto *region = get_region_settings( copy_from ) ) {
            for( const auto &extra : region->region_extras ) {
                result.extras.emplace_back( extra.first );
            }
        } else {
            jo.throw_error( string_format( "unknown region_settings_map_extras copy-from '%s'", copy_from ),
                            "copy-from" );
        }
        result.copy_from = copy_from;
    }
    read_string_list( jo, "extras", result.extras, true );
    if( jo.has_object( "extend" ) ) {
        read_string_list( jo.get_object( "extend" ), "extras", result.extras, false );
    }
    compat_region_settings_map_extras_defs[id] = result;
}

auto load_compat_region_terrain_furniture( const JsonObject &jo ) -> void
{
    const auto id = jo.get_string( "id" );
    auto result = compat_region_terrain_furniture {};
    read_compat_copy_from( jo, compat_region_terrain_furniture_defs, result,
                           "region_terrain_furniture" );
    if( jo.has_string( "ter_id" ) ) {
        result.replaced_terrain_id = jo.get_string( "ter_id" );
    }
    if( jo.has_string( "furn_id" ) ) {
        result.replaced_furniture_id = jo.get_string( "furn_id" );
    }
    if( jo.has_array( "replace_with_terrain" ) ) {
        result.terrain = weighted_int_list<ter_id> {};
        for( JsonArray entry : jo.get_array( "replace_with_terrain" ) ) {
            result.terrain.add_or_replace( ter_id( entry.get_string( 0 ) ), entry.get_int( 1 ) );
        }
    }
    if( jo.has_array( "replace_with_furniture" ) ) {
        result.furniture = weighted_int_list<furn_id> {};
        for( JsonArray entry : jo.get_array( "replace_with_furniture" ) ) {
            result.furniture.add_or_replace( furn_id( entry.get_string( 0 ) ), entry.get_int( 1 ) );
        }
    }
    compat_region_terrain_furniture_defs[id] = result;
}

auto load_compat_region_settings_terrain_furniture( const JsonObject &jo ) -> void
{
    const auto id = jo.get_string( "id" );
    auto result = compat_region_settings_terrain_furniture {};
    if( jo.has_string( "copy-from" ) ) {
        const auto copy_from = jo.get_string( "copy-from" );
        if( const auto iter = compat_region_settings_terrain_furniture_defs.find( copy_from );
            iter != compat_region_settings_terrain_furniture_defs.end() ) {
            result = iter->second;
        } else if( get_region_settings( copy_from ) == nullptr ) {
            jo.throw_error( string_format( "unknown region_settings_terrain_furniture copy-from '%s'",
                                           copy_from ), "copy-from" );
        }
        result.copy_from = copy_from;
    }
    read_string_list( jo, "ter_furn", result.ter_furn, true );
    if( jo.has_object( "extend" ) ) {
        read_string_list( jo.get_object( "extend" ), "ter_furn", result.ter_furn, false );
    }
    compat_region_settings_terrain_furniture_defs[id] = result;
}

auto load_compat_region_settings_forest_mapgen( const JsonObject &jo ) -> void
{
    const auto id = jo.get_string( "id" );
    auto result = compat_region_settings_forest_mapgen {};
    if( jo.has_string( "copy-from" ) ) {
        const auto copy_from = jo.get_string( "copy-from" );
        if( const auto iter = compat_region_settings_forest_mapgen_defs.find( copy_from );
            iter != compat_region_settings_forest_mapgen_defs.end() ) {
            result = iter->second;
        } else if( get_region_settings( copy_from ) == nullptr ) {
            jo.throw_error( string_format( "unknown region_settings_forest_mapgen copy-from '%s'",
                                           copy_from ), "copy-from" );
        }
        result.copy_from = copy_from;
    }
    read_string_list( jo, "biomes", result.biomes, true );
    if( jo.has_object( "extend" ) ) {
        read_string_list( jo.get_object( "extend" ), "biomes", result.biomes, false );
    }
    compat_region_settings_forest_mapgen_defs[id] = result;
}

auto load_compat_region_settings_city( const JsonObject &jo ) -> void
{
    const auto id = jo.get_string( "id" );
    auto result = compat_region_settings_city {};
    if( jo.has_string( "copy-from" ) ) {
        const auto copy_from = jo.get_string( "copy-from" );
        if( const auto iter = compat_region_settings_city_defs.find( copy_from );
            iter != compat_region_settings_city_defs.end() ) {
            result = iter->second;
        } else if( const auto city = get_region_city_settings( copy_from ) ) {
            result.city = *city;
        } else {
            jo.throw_error( string_format( "unknown region_settings_city copy-from '%s'", copy_from ),
                            "copy-from" );
        }
        result.copy_from = copy_from;
    }
    jo.read( "city_size", result.city.city_size );
    if( jo.has_bool( "is_megacity" ) ) {
        ( void )jo.get_bool( "is_megacity" );
    }
    if( jo.has_string( "name_snippet" ) ) {
        ( void )jo.get_string( "name_snippet" );
    }
    jo.read( "city_spacing", result.city.city_spacing );
    jo.read( "shop_radius", result.city.shop_radius );
    jo.read( "shop_sigma", result.city.shop_sigma );
    jo.read( "park_radius", result.city.park_radius );
    jo.read( "park_sigma", result.city.park_sigma );
    read_city_buildings( jo, "houses", result.city.houses );
    read_city_buildings( jo, "shops", result.city.shops );
    read_city_buildings( jo, "parks", result.city.parks );
    read_city_buildings( jo, "finales", result.city.finales );
    if( jo.has_object( "extend" ) ) {
        const auto extend = jo.get_object( "extend" );
        read_city_buildings( extend, "houses", result.city.houses );
        read_city_buildings( extend, "shops", result.city.shops );
        read_city_buildings( extend, "parks", result.city.parks );
        read_city_buildings( extend, "finales", result.city.finales );
    }
    compat_region_settings_city_defs[id] = result;
}

auto load_compat_region_settings_forest( const JsonObject &jo ) -> void
{
    const auto id = jo.get_string( "id" );
    auto result = compat_region_settings_forest {};
    if( jo.has_string( "copy-from" ) ) {
        const auto copy_from = jo.get_string( "copy-from" );
        if( const auto iter = compat_region_settings_forest_defs.find( copy_from );
            iter != compat_region_settings_forest_defs.end() ) {
            result = iter->second;
        } else if( const auto forest = get_region_forest_settings( copy_from ) ) {
            result.forest = *forest;
        } else {
            jo.throw_error( string_format( "unknown region_settings_forest copy-from '%s'", copy_from ),
                            "copy-from" );
        }
        result.copy_from = copy_from;
    }
    jo.read( "noise_threshold_forest", result.forest.noise_threshold_forest );
    jo.read( "noise_threshold_forest_thick", result.forest.noise_threshold_forest_thick );
    jo.read( "noise_threshold_swamp_adjacent_water",
             result.forest.noise_threshold_swamp_adjacent_water );
    jo.read( "noise_threshold_swamp_isolated", result.forest.noise_threshold_swamp_isolated );
    jo.read( "river_floodplain_buffer_distance_min",
             result.forest.river_floodplain_buffer_distance_min );
    jo.read( "river_floodplain_buffer_distance_max",
             result.forest.river_floodplain_buffer_distance_max );
    if( jo.has_number( "forest_threshold_limit" ) ) {
        ( void )jo.get_float( "forest_threshold_limit" );
    }
    if( jo.has_array( "forest_threshold_increase" ) ) {
        ( void )jo.get_array( "forest_threshold_increase" );
    }
    compat_region_settings_forest_defs[id] = result;
}

[[noreturn]] auto throw_unknown_compat_ref( const std::string &type_name,
        const std::string &id ) -> void
{
    throw std::runtime_error( string_format( "unknown %s '%s'", type_name, id ) );
}

auto apply_compat_map_extras_ref( regional_settings &region,
                                  const std::string &map_extras_id ) -> void
{
    const auto settings_iter = compat_region_settings_map_extras_defs.find( map_extras_id );
    if( settings_iter == compat_region_settings_map_extras_defs.end() ) {
        throw_unknown_compat_ref( "region_settings_map_extras", map_extras_id );
    }
    for( const auto &extra_id : settings_iter->second.extras ) {
        if( const auto extra_iter = compat_map_extra_collection_defs.find( extra_id );
            extra_iter != compat_map_extra_collection_defs.end() ) {
            auto extras = map_extras { extra_iter->second.chance };
            extras.values = extra_iter->second.values;
            region.region_extras[extra_id] = extras;
        } else if( !region.region_extras.contains( extra_id ) ) {
            throw_unknown_compat_ref( "map_extra_collection", extra_id );
        }
    }
}

auto apply_compat_forest_mapgen_ref( regional_settings &region,
                                     const std::string &forest_composition_id ) -> void
{
    const auto settings_iter = compat_region_settings_forest_mapgen_defs.find( forest_composition_id );
    if( settings_iter == compat_region_settings_forest_mapgen_defs.end() ) {
        throw_unknown_compat_ref( "region_settings_forest_mapgen", forest_composition_id );
    }
    const auto preserve_existing = settings_iter->second.copy_from &&
                                   get_region_settings( *settings_iter->second.copy_from ) != nullptr;
    if( !preserve_existing ) {
        region.forest_composition.unfinalized_biomes.clear();
    }
    for( const auto &biome_id : settings_iter->second.biomes ) {
        const auto biome_iter = compat_forest_biome_mapgen_defs.find( biome_id );
        if( biome_iter == compat_forest_biome_mapgen_defs.end() ) {
            throw_unknown_compat_ref( "forest_biome_mapgen", biome_id );
        }
        const auto biome = resolve_compat_forest_biome( biome_iter->second );
        for( const auto &terrain : biome_iter->second.terrains ) {
            region.forest_composition.unfinalized_biomes[terrain] = biome;
        }
    }
}

auto apply_compat_terrain_furniture_ref( regional_settings &region,
        const std::string &terrain_furniture_id ) -> void
{
    const auto settings_iter = compat_region_settings_terrain_furniture_defs.find(
                                   terrain_furniture_id );
    if( settings_iter == compat_region_settings_terrain_furniture_defs.end() ) {
        throw_unknown_compat_ref( "region_settings_terrain_furniture", terrain_furniture_id );
    }
    for( const auto &ter_furn_id : settings_iter->second.ter_furn ) {
        const auto ter_furn_iter = compat_region_terrain_furniture_defs.find( ter_furn_id );
        if( ter_furn_iter == compat_region_terrain_furniture_defs.end() ) {
            throw_unknown_compat_ref( "region_terrain_furniture", ter_furn_id );
        }
        const auto &ter_furn = ter_furn_iter->second;
        if( ter_furn.replaced_terrain_id ) {
            for( const auto &replacement : ter_furn.terrain ) {
                region.region_terrain_and_furniture.unfinalized_terrain[*ter_furn.replaced_terrain_id]
                [replacement.obj.id().str()] = replacement.weight;
            }
        }
        if( ter_furn.replaced_furniture_id ) {
            for( const auto &replacement : ter_furn.furniture ) {
                region.region_terrain_and_furniture.unfinalized_furniture[*ter_furn.replaced_furniture_id]
                [replacement.obj.id().str()] = replacement.weight;
            }
        }
    }
}

auto apply_compat_region_references( regional_settings &region,
                                     const compat_region_references &refs ) -> void
{
    if( refs.cities ) {
        if( const auto city_iter = compat_region_settings_city_defs.find( *refs.cities );
            city_iter != compat_region_settings_city_defs.end() ) {
            region.city_spec = city_iter->second.city;
        } else if( *refs.cities != "no_cities" ) {
            throw_unknown_compat_ref( "region_settings_city", *refs.cities );
        }
    }
    if( refs.weather ) {
        if( const auto weather_iter = compat_weather_generators.find( *refs.weather );
            weather_iter != compat_weather_generators.end() ) {
            region.weather = weather_iter->second;
        } else {
            throw_unknown_compat_ref( "weather_generator", *refs.weather );
        }
    }
    if( refs.map_extras ) {
        apply_compat_map_extras_ref( region, *refs.map_extras );
    }
    if( refs.forest_composition ) {
        apply_compat_forest_mapgen_ref( region, *refs.forest_composition );
    }
    if( refs.forests ) {
        if( const auto forest_iter = compat_region_settings_forest_defs.find( *refs.forests );
            forest_iter != compat_region_settings_forest_defs.end() ) {
            region.overmap_forest = forest_iter->second.forest;
        } else {
            throw_unknown_compat_ref( "region_settings_forest", *refs.forests );
        }
    }
    if( refs.terrain_furniture ) {
        apply_compat_terrain_furniture_ref( region, *refs.terrain_furniture );
    }
}

auto record_compat_region_reference( const std::string &region_id, const std::string &member,
                                     const std::string &value ) -> void
{
    auto &refs = compat_region_refs[region_id];
    if( member == "cities" ) {
        refs.cities = value;
    } else if( member == "weather" ) {
        refs.weather = value;
    } else if( member == "map_extras" ) {
        refs.map_extras = value;
    } else if( member == "forest_composition" ) {
        refs.forest_composition = value;
    } else if( member == "forests" ) {
        refs.forests = value;
    } else if( member == "terrain_furniture" ) {
        refs.terrain_furniture = value;
    }
}

static void load_forest_biome_component(
    const JsonObject &jo, forest_biome_component &forest_biome_component, const bool overlay )
{
    read_and_set_or_throw<int>( jo, "chance", forest_biome_component.chance, !overlay );
    read_and_set_or_throw<int>( jo, "sequence", forest_biome_component.sequence, !overlay );
    read_and_set_or_throw<bool>( jo, "clear_types", forest_biome_component.clear_types, !overlay );

    if( forest_biome_component.clear_types ) {
        forest_biome_component.unfinalized_types.clear();
    }

    if( !jo.has_object( "types" ) ) {
        if( !overlay ) {
            jo.throw_error( "types required" );
        }
    } else {
        for( const JsonMember member : jo.get_object( "types" ) ) {
            if( member.is_comment() ) {
                continue;
            }
            forest_biome_component.unfinalized_types[member.name()] = member.get_int();
        }
    }
}

static void load_forest_biome_terrain_dependent_furniture( const JsonObject &jo,
        forest_biome_terrain_dependent_furniture &forest_biome_terrain_dependent_furniture,
        const bool overlay )
{
    read_and_set_or_throw<int>( jo, "chance", forest_biome_terrain_dependent_furniture.chance,
                                !overlay );
    read_and_set_or_throw<bool>( jo, "clear_furniture",
                                 forest_biome_terrain_dependent_furniture.clear_furniture, !overlay );

    if( forest_biome_terrain_dependent_furniture.clear_furniture ) {
        forest_biome_terrain_dependent_furniture.unfinalized_furniture.clear();
    }

    if( !jo.has_object( "furniture" ) ) {
        if( !overlay ) {
            jo.throw_error( "furniture required" );
        }
    } else {
        for( const JsonMember member : jo.get_object( "furniture" ) ) {
            if( member.is_comment() ) {
                continue;
            }
            forest_biome_terrain_dependent_furniture.unfinalized_furniture[member.name()] = member.get_int();
        }
    }
}

static void load_forest_biome( const JsonObject &jo, forest_biome &forest_biome,
                               const bool overlay )
{
    read_and_set_or_throw<int>( jo, "sparseness_adjacency_factor",
                                forest_biome.sparseness_adjacency_factor, !overlay );
    read_and_set_or_throw<item_group_id>( jo, "item_group", forest_biome.item_group, !overlay );
    read_and_set_or_throw<int>( jo, "item_group_chance", forest_biome.item_group_chance, !overlay );
    read_and_set_or_throw<int>( jo, "item_spawn_iterations", forest_biome.item_spawn_iterations,
                                !overlay );
    read_and_set_or_throw<bool>( jo, "clear_components", forest_biome.clear_components, !overlay );
    read_and_set_or_throw<bool>( jo, "clear_groundcover", forest_biome.clear_groundcover, !overlay );
    read_and_set_or_throw<bool>( jo, "clear_terrain_furniture", forest_biome.clear_terrain_furniture,
                                 !overlay );

    if( forest_biome.clear_components ) {
        forest_biome.unfinalized_biome_components.clear();
    }

    if( !jo.has_object( "components" ) ) {
        if( !overlay ) {
            jo.throw_error( "components required" );
        }
    } else {
        for( const JsonMember member : jo.get_object( "components" ) ) {
            if( member.is_comment() ) {
                continue;
            }
            JsonObject component_jo = member.get_object();
            load_forest_biome_component( component_jo, forest_biome.unfinalized_biome_components[member.name()],
                                         overlay );
        }
    }

    if( forest_biome.clear_groundcover ) {
        forest_biome.unfinalized_groundcover.clear();
    }

    if( !jo.has_object( "groundcover" ) ) {
        if( !overlay ) {
            jo.throw_error( "groundcover required" );
        }
    } else {
        for( const JsonMember member : jo.get_object( "groundcover" ) ) {
            if( member.is_comment() ) {
                continue;
            }
            forest_biome.unfinalized_groundcover[member.name()] = member.get_int();
        }
    }

    if( !jo.has_object( "terrain_furniture" ) ) {
        if( !overlay ) {
            jo.throw_error( "terrain_furniture required" );
        }
    } else {
        for( const JsonMember member : jo.get_object( "terrain_furniture" ) ) {
            if( member.is_comment() ) {
                continue;
            }
            JsonObject terrain_furniture_jo = member.get_object();
            load_forest_biome_terrain_dependent_furniture( terrain_furniture_jo,
                    forest_biome.unfinalized_terrain_dependent_furniture[member.name()], overlay );
        }
    }
}

static void load_forest_mapgen_settings( const JsonObject &jo,
        forest_mapgen_settings &forest_mapgen_settings,
        const bool strict,
        const bool overlay )
{
    if( !jo.has_object( "forest_mapgen_settings" ) ) {
        if( strict ) {
            jo.throw_error( "\"forest_mapgen_settings\": { … } required for default" );
        }
    } else {
        for( const JsonMember member : jo.get_object( "forest_mapgen_settings" ) ) {
            if( member.is_comment() ) {
                continue;
            }
            JsonObject forest_biome_jo = member.get_object();
            load_forest_biome( forest_biome_jo, forest_mapgen_settings.unfinalized_biomes[member.name()],
                               overlay );
        }
    }
}

static void load_forest_trail_settings( const JsonObject &jo,
                                        forest_trail_settings &forest_trail_settings,
                                        const bool strict, const bool overlay )
{
    if( !jo.has_object( "forest_trail_settings" ) ) {
        if( strict ) {
            jo.throw_error( "\"forest_trail_settings\": { … } required for default" );
        }
    } else {
        JsonObject forest_trail_settings_jo = jo.get_object( "forest_trail_settings" );
        read_and_set_or_throw<int>( forest_trail_settings_jo, "chance", forest_trail_settings.chance,
                                    !overlay );
        read_and_set_or_throw<int>( forest_trail_settings_jo, "border_point_chance",
                                    forest_trail_settings.border_point_chance, !overlay );
        read_and_set_or_throw<int>( forest_trail_settings_jo, "minimum_forest_size",
                                    forest_trail_settings.minimum_forest_size, !overlay );
        read_and_set_or_throw<int>( forest_trail_settings_jo, "random_point_min",
                                    forest_trail_settings.random_point_min, !overlay );
        read_and_set_or_throw<int>( forest_trail_settings_jo, "random_point_max",
                                    forest_trail_settings.random_point_max, !overlay );
        read_and_set_or_throw<int>( forest_trail_settings_jo, "random_point_size_scalar",
                                    forest_trail_settings.random_point_size_scalar, !overlay );
        read_and_set_or_throw<int>( forest_trail_settings_jo, "trailhead_chance",
                                    forest_trail_settings.trailhead_chance, !overlay );
        read_and_set_or_throw<int>( forest_trail_settings_jo, "trailhead_road_distance",
                                    forest_trail_settings.trailhead_road_distance, !overlay );
        read_and_set_or_throw<int>( forest_trail_settings_jo, "trail_center_variance",
                                    forest_trail_settings.trail_center_variance, !overlay );
        read_and_set_or_throw<int>( forest_trail_settings_jo, "trail_width_offset_min",
                                    forest_trail_settings.trail_width_offset_min, !overlay );
        read_and_set_or_throw<int>( forest_trail_settings_jo, "trail_width_offset_max",
                                    forest_trail_settings.trail_width_offset_max, !overlay );
        read_and_set_or_throw<bool>( forest_trail_settings_jo, "clear_trail_terrain",
                                     forest_trail_settings.clear_trail_terrain, !overlay );

        if( forest_trail_settings.clear_trail_terrain ) {
            forest_trail_settings.unfinalized_trail_terrain.clear();
        }

        if( !forest_trail_settings_jo.has_object( "trail_terrain" ) ) {
            if( !overlay ) {
                forest_trail_settings_jo.throw_error( "trail_terrain required" );
            }
        } else {
            for( const JsonMember member : forest_trail_settings_jo.get_object( "trail_terrain" ) ) {
                if( member.is_comment() ) {
                    continue;
                }
                forest_trail_settings.unfinalized_trail_terrain[member.name()] = member.get_int();
            }
        }

        if( !forest_trail_settings_jo.has_object( "trailheads" ) ) {
            if( !overlay ) {
                forest_trail_settings_jo.throw_error( "trailheads required" );
            }
        } else {
            for( const JsonMember member : forest_trail_settings_jo.get_object( "trailheads" ) ) {
                if( member.is_comment() ) {
                    continue;
                }
                forest_trail_settings.trailheads.add( overmap_special_id( member.name() ), member.get_int() );
            }
        }
    }
}

static void load_overmap_feature_flag_settings( const JsonObject &jo,
        overmap_feature_flag_settings &overmap_feature_flag_settings,
        const bool strict, const bool overlay )
{
    if( !jo.has_object( "overmap_feature_flag_settings" ) ) {
        if( strict ) {
            jo.throw_error( "\"overmap_feature_flag_settings\": { … } required for default" );
        }
    } else {
        JsonObject overmap_feature_flag_settings_jo = jo.get_object( "overmap_feature_flag_settings" );
        read_and_set_or_throw<bool>( overmap_feature_flag_settings_jo, "clear_blacklist",
                                     overmap_feature_flag_settings.clear_blacklist, !overlay );
        read_and_set_or_throw<bool>( overmap_feature_flag_settings_jo, "clear_whitelist",
                                     overmap_feature_flag_settings.clear_whitelist, !overlay );

        if( overmap_feature_flag_settings.clear_blacklist ) {
            overmap_feature_flag_settings.blacklist.clear();
        }

        if( overmap_feature_flag_settings.clear_whitelist ) {
            overmap_feature_flag_settings.whitelist.clear();
        }

        if( !overmap_feature_flag_settings_jo.has_array( "blacklist" ) ) {
            if( !overlay ) {
                overmap_feature_flag_settings_jo.throw_error( "blacklist required" );
            }
        } else {
            for( const std::string line : overmap_feature_flag_settings_jo.get_array( "blacklist" ) ) {
                overmap_feature_flag_settings.blacklist.emplace( line );
            }
        }

        if( !overmap_feature_flag_settings_jo.has_array( "whitelist" ) ) {
            if( !overlay ) {
                overmap_feature_flag_settings_jo.throw_error( "whitelist required" );
            }
        } else {
            for( const std::string line : overmap_feature_flag_settings_jo.get_array( "whitelist" ) ) {
                overmap_feature_flag_settings.whitelist.emplace( line );
            }
        }
    }
}

static void load_overmap_forest_settings(
    const JsonObject &jo, overmap_forest_settings &overmap_forest_settings, const bool strict,
    const bool overlay )
{
    if( !jo.has_object( "overmap_forest_settings" ) ) {
        if( strict ) {
            jo.throw_error( "\"overmap_forest_settings\": { … } required for default" );
        }
    } else {
        JsonObject overmap_forest_settings_jo = jo.get_object( "overmap_forest_settings" );
        read_and_set_or_throw<double>( overmap_forest_settings_jo, "noise_threshold_forest",
                                       overmap_forest_settings.noise_threshold_forest, !overlay );
        read_and_set_or_throw<double>( overmap_forest_settings_jo, "noise_threshold_forest_thick",
                                       overmap_forest_settings.noise_threshold_forest_thick, !overlay );
        read_and_set_or_throw<double>( overmap_forest_settings_jo, "noise_threshold_swamp_adjacent_water",
                                       overmap_forest_settings.noise_threshold_swamp_adjacent_water, !overlay );
        read_and_set_or_throw<double>( overmap_forest_settings_jo, "noise_threshold_swamp_isolated",
                                       overmap_forest_settings.noise_threshold_swamp_isolated, !overlay );
        read_and_set_or_throw<int>( overmap_forest_settings_jo, "river_floodplain_buffer_distance_min",
                                    overmap_forest_settings.river_floodplain_buffer_distance_min, !overlay );
        read_and_set_or_throw<int>( overmap_forest_settings_jo, "river_floodplain_buffer_distance_max",
                                    overmap_forest_settings.river_floodplain_buffer_distance_max, !overlay );
    }
}

static void load_overmap_lake_settings( const JsonObject &jo,
                                        overmap_lake_settings &overmap_lake_settings,
                                        const bool strict, const bool overlay )
{
    if( !jo.has_object( "overmap_lake_settings" ) ) {
        if( strict ) {
            jo.throw_error( "\"overmap_lake_settings\": { … } required for default" );
        }
    } else {
        JsonObject overmap_lake_settings_jo = jo.get_object( "overmap_lake_settings" );
        read_and_set_or_throw<double>( overmap_lake_settings_jo, "noise_threshold_lake",
                                       overmap_lake_settings.noise_threshold_lake, !overlay );
        read_and_set_or_throw<int>( overmap_lake_settings_jo, "lake_size_min",
                                    overmap_lake_settings.lake_size_min, !overlay );
        read_and_set_or_throw<int>( overmap_lake_settings_jo, "lake_depth",
                                    overmap_lake_settings.lake_depth, !overlay );

        if( !overmap_lake_settings_jo.has_array( "shore_extendable_overmap_terrain" ) ) {
            if( !overlay ) {
                overmap_lake_settings_jo.throw_error( "shore_extendable_overmap_terrain required" );
            }
        } else {
            const std::vector<std::string> from_json =
                overmap_lake_settings_jo.get_string_array( "shore_extendable_overmap_terrain" );
            overmap_lake_settings.unfinalized_shore_extendable_overmap_terrain.insert(
                overmap_lake_settings.unfinalized_shore_extendable_overmap_terrain.end(), from_json.begin(),
                from_json.end() );
        }

        if( !overmap_lake_settings_jo.has_array( "shore_extendable_overmap_terrain_aliases" ) ) {
            if( !overlay ) {
                overmap_lake_settings_jo.throw_error( "shore_extendable_overmap_terrain_aliases required" );
            }
        } else {
            for( JsonObject alias_entry :
                 overmap_lake_settings_jo.get_array( "shore_extendable_overmap_terrain_aliases" ) ) {
                shore_extendable_overmap_terrain_alias alias;
                alias_entry.read( "om_terrain", alias.overmap_terrain );
                alias_entry.read( "alias", alias.alias );
                alias.match_type = alias_entry.get_enum_value<ot_match_type>( "om_terrain_match_type",
                                   ot_match_type::contains );
                overmap_lake_settings.shore_extendable_overmap_terrain_aliases.emplace_back( alias );
            }
        }
    }
}

static void load_region_terrain_and_furniture_settings( const JsonObject &jo,
        region_terrain_and_furniture_settings &region_terrain_and_furniture_settings,
        const bool strict, const bool overlay )
{
    if( !jo.has_object( "region_terrain_and_furniture" ) ) {
        if( strict ) {
            jo.throw_error( "\"region_terrain_and_furniture\": { … } required for default" );
        }
    } else {
        JsonObject region_terrain_and_furniture_settings_jo =
            jo.get_object( "region_terrain_and_furniture" );

        if( !region_terrain_and_furniture_settings_jo.has_object( "terrain" ) ) {
            if( !overlay ) {
                region_terrain_and_furniture_settings_jo.throw_error( "terrain required" );
            }
        } else {
            for( const JsonMember region : region_terrain_and_furniture_settings_jo.get_object( "terrain" ) ) {
                if( region.is_comment() ) {
                    continue;
                }
                for( const JsonMember terrain : region.get_object() ) {
                    if( terrain.is_comment() ) {
                        continue;
                    }
                    region_terrain_and_furniture_settings.unfinalized_terrain[region.name()][terrain.name()] =
                        terrain.get_int();
                }
            }
        }

        if( !region_terrain_and_furniture_settings_jo.has_object( "furniture" ) ) {
            if( !overlay ) {
                region_terrain_and_furniture_settings_jo.throw_error( "furniture required" );
            }
        } else {
            for( const JsonMember template_furniture :
                 region_terrain_and_furniture_settings_jo.get_object( "furniture" ) ) {
                if( template_furniture.is_comment() ) {
                    continue;
                }
                for( const JsonMember furniture : template_furniture.get_object() ) {
                    if( furniture.is_comment() ) {
                        continue;
                    }
                    region_terrain_and_furniture_settings.unfinalized_furniture[template_furniture.name()][furniture.name()]
                        = furniture.get_int();
                }
            }
        }
    }
}

void load_region_settings( const JsonObject &jo )
{
    regional_settings new_region;
    if( !jo.read( "id", new_region.id ) ) {
        jo.throw_error( "No 'id' field." );
    }
    const auto region_id = new_region.id;
    if( jo.has_string( "copy-from" ) ) {
        const auto copy_from = jo.get_string( "copy-from" );
        if( const auto iter = region_settings_map.find( copy_from ); iter != region_settings_map.end() ) {
            new_region = iter->second;
            new_region.id = region_id;
        } else {
            jo.throw_error( string_format( "unknown region_settings copy-from '%s'", copy_from ), "copy-from" );
        }
    }
    const auto strict = new_region.id == "default";
    if( !read_default_oter( jo, new_region.default_oter ) && strict ) {
        jo.throw_error( "default_oter required for default ( though it should probably remain 'field' )" );
    }
    if( !jo.read( "river_scale", new_region.river_scale ) && strict ) {
        jo.throw_error( "river_scale required for default" );
    }
    if( jo.has_array( "default_groundcover" ) ) {
        new_region.default_groundcover_str.reset( new weighted_int_list<ter_str_id> );
        for( JsonArray inner : jo.get_array( "default_groundcover" ) ) {
            if( new_region.default_groundcover_str->add( ter_str_id( inner.get_string( 0 ) ),
                    inner.get_int( 1 ) ) == nullptr ) {
                jo.throw_error( "'default_groundcover' must be a weighted list: an array of pairs [ \"id\", weight ]" );
            }
        }
    } else if( strict ) {
        jo.throw_error( "Weighted list 'default_groundcover' required for 'default'" );
    }

    if( !jo.has_object( "field_coverage" ) ) {
        if( strict ) {
            jo.throw_error( "\"field_coverage\": { … } required for default" );
        }
    } else {
        JsonObject pjo = jo.get_object( "field_coverage" );
        double tmpval = 0.0f;
        if( !pjo.read( "percent_coverage", tmpval ) ) {
            pjo.throw_error( "field_coverage: percent_coverage required" );
        }
        new_region.field_coverage.mpercent_coverage = static_cast<int>( tmpval * 10000.0 );
        if( !pjo.read( "default_ter", new_region.field_coverage.default_ter_str ) ) {
            pjo.throw_error( "field_coverage: default_ter required" );
        }
        tmpval = 0.0f;
        if( pjo.has_object( "other" ) ) {
            for( const JsonMember member : pjo.get_object( "other" ) ) {
                if( member.is_comment() ) {
                    continue;
                }
                new_region.field_coverage.percent_str[member.name()] = member.get_float();
            }
        }
        if( pjo.read( "boost_chance", tmpval ) && tmpval != 0.0f ) {
            new_region.field_coverage.boost_chance = static_cast<int>( tmpval * 10000.0 );
            if( !pjo.read( "boosted_percent_coverage", tmpval ) ) {
                pjo.throw_error( "boost_chance > 0 requires boosted_percent_coverage" );
            }
            new_region.field_coverage.boosted_mpercent_coverage = static_cast<int>( tmpval * 10000.0 );
            if( !pjo.read( "boosted_other_percent", tmpval ) ) {
                pjo.throw_error( "boost_chance > 0 requires boosted_other_percent" );
            }
            new_region.field_coverage.boosted_other_mpercent = static_cast<int>( tmpval * 10000.0 );
            if( pjo.has_object( "boosted_other" ) ) {
                for( const JsonMember member : pjo.get_object( "boosted_other" ) ) {
                    if( member.is_comment() ) {
                        continue;
                    }
                    new_region.field_coverage.boosted_percent_str[member.name()] = member.get_float();
                }
            } else {
                pjo.throw_error( "boost_chance > 0 requires boosted_other { … }" );
            }
        }
    }

    const auto strict_forest_mapgen = strict && !jo.has_string( "forest_composition" );
    load_forest_mapgen_settings( jo, new_region.forest_composition, strict_forest_mapgen, false );
    if( jo.has_string( "forest_composition" ) ) {
        record_compat_region_reference( region_id, "forest_composition",
                                        jo.get_string( "forest_composition" ) );
    }

    const auto strict_forest_trail = strict && !jo.has_member( "forest_trails" );
    load_forest_trail_settings( jo, new_region.forest_trail, strict_forest_trail, false );

    const auto strict_map_extras = strict && !jo.has_string( "map_extras" );
    if( !jo.has_object( "map_extras" ) ) {
        if( strict_map_extras ) {
            jo.throw_error( "\"map_extras\": { … } required for default" );
        }
    } else {
        for( const JsonMember zone : jo.get_object( "map_extras" ) ) {
            if( zone.is_comment() ) {
                continue;
            }
            JsonObject zjo = zone.get_object();
            map_extras extras( 0 );

            if( !zjo.read( "chance", extras.chance ) && strict_map_extras ) {
                zjo.throw_error( "chance required for default" );
            }

            if( !zjo.has_object( "extras" ) ) {
                if( strict_map_extras ) {
                    zjo.throw_error( "\"extras\": { … } required for default" );
                }
            } else {
                for( const JsonMember member : zjo.get_object( "extras" ) ) {
                    if( member.is_comment() ) {
                        continue;
                    }
                    extras.values.add( member.name(), member.get_int() );
                }
            }

            new_region.region_extras[zone.name()] = extras;
        }
    }
    if( jo.has_string( "map_extras" ) ) {
        record_compat_region_reference( region_id, "map_extras", jo.get_string( "map_extras" ) );
    }

    if( !jo.has_object( "city" ) ) {
        if( strict && !jo.has_string( "cities" ) ) {
            jo.throw_error( "\"city\": { … } required for default" );
        }
        if( jo.has_string( "cities" ) ) {
            const auto cities_id = jo.get_string( "cities" );
            if( cities_id == "no_cities" ) {
                new_region.city_spec.city_size = 0;
                new_region.city_spec.city_spacing = 0;
            } else {
                record_compat_region_reference( region_id, "cities", cities_id );
            }
        }
    } else {
        JsonObject cjo = jo.get_object( "city" );
        cjo.read( "city_size", new_region.city_spec.city_size );
        cjo.read( "city_spacing", new_region.city_spec.city_spacing );
        if( !cjo.read( "shop_radius", new_region.city_spec.shop_radius ) && strict ) {
            jo.throw_error( "city: shop_radius required for default" );
        }
        if( !cjo.read( "shop_sigma", new_region.city_spec.shop_sigma ) && strict ) {
            jo.throw_error( "city: shop_sigma required for default" );
        }
        if( !cjo.read( "park_radius", new_region.city_spec.park_radius ) && strict ) {
            jo.throw_error( "city: park_radius required for default" );
        }
        if( !cjo.read( "park_sigma", new_region.city_spec.park_sigma ) && strict ) {
            jo.throw_error( "city: park_sigma required for default" );
        }
        const auto load_building_types = [&jo, &cjo, strict]( const std::string & type,
        building_bin & dest ) {
            if( !cjo.has_object( type ) && strict ) {
                jo.throw_error( "city: \"" + type + "\": { … } required for default" );
            } else {
                for( const JsonMember member : cjo.get_object( type ) ) {
                    if( member.is_comment() ) {
                        continue;
                    }
                    dest.add( overmap_special_id( member.name() ), member.get_int() );
                }
            }
        };
        load_building_types( "houses", new_region.city_spec.houses );
        load_building_types( "urban_houses", new_region.city_spec.urban_houses );
        load_building_types( "shops", new_region.city_spec.shops );
        load_building_types( "urban_shops", new_region.city_spec.urban_shops );
        load_building_types( "parks", new_region.city_spec.parks );
        if( cjo.has_member( "finales" ) ) {
            load_building_types( "finales", new_region.city_spec.finales );
        }
    }

    if( !jo.has_object( "weather" ) ) {
        if( strict && !jo.has_string( "weather" ) ) {
            jo.throw_error( "\"weather\": { … } required for default" );
        }
        if( jo.has_string( "weather" ) ) {
            record_compat_region_reference( region_id, "weather", jo.get_string( "weather" ) );
        }
    } else {
        JsonObject wjo = jo.get_object( "weather" );
        new_region.weather = weather_generator::load( wjo );
    }

    // Unclear if required. C++ uninitialized values now concern me.
    new_region.region_effects = {};
    for( const auto effect_type : all_enum_values<region_effect_type>() ) {
        new_region.region_effects[effect_type] = {};
    }

    if( jo.has_array( "effects" ) ) {
        JsonArray effects = jo.get_array( "effects" );
        for( JsonObject effect_object : effects ) {
            auto effect_type = region_effect_type::generic;
            efftype_id effect_id( effect_object.get_string( "effect_id" ) );
            int one_in = 0;
            if( effect_object.has_int( "one_in" ) ) {
                one_in = effect_object.get_int( "one_in" );
            }
            if( effect_object.has_string( "effect_type" ) ) {
                if( effect_object.get_string( "effect_type" ) == "generic" ) {
                    effect_type = region_effect_type::generic;
                } else if( effect_object.get_string( "effect_type" ) == "night_time" ) {
                    effect_type = region_effect_type::night_time;
                } else if( effect_object.get_string( "effect_type" ) == "sunlight" ) {
                    effect_type = region_effect_type::sunlight;
                } else if( effect_object.get_string( "effect_type" ) == "surface" ) {
                    effect_type = region_effect_type::surface;
                } else if( effect_object.get_string( "effect_type" ) == "underground" ) {
                    effect_type = region_effect_type::underground;
                } else if( effect_object.get_string( "effect_type" ) == "underwater" ) {
                    effect_type = region_effect_type::underwater;
                } else if( effect_object.get_string( "effect_type" ) == "sleep" ) {
                    effect_type = region_effect_type::sleep;
                } else {
                    debugmsg( "Unknown effect type: %s", effect_object.get_string( "effect_type" ) );
                }
            }
            std::pair<efftype_id, int> effect( effect_id, one_in );
            new_region.region_effects[effect_type].emplace_back( effect );
        }
    }

    if( jo.has_object( "feature_flag_settings" ) ) {
        auto feature_flag_settings = jo.get_object( "feature_flag_settings" );
        read_and_set_or_throw<bool>( feature_flag_settings, "clear_blacklist",
                                     new_region.overmap_feature_flag.clear_blacklist, false );
        read_and_set_or_throw<bool>( feature_flag_settings, "clear_whitelist",
                                     new_region.overmap_feature_flag.clear_whitelist, false );
        if( new_region.overmap_feature_flag.clear_blacklist ) {
            new_region.overmap_feature_flag.blacklist.clear();
        }
        if( new_region.overmap_feature_flag.clear_whitelist ) {
            new_region.overmap_feature_flag.whitelist.clear();
        }
        for( const auto &value : feature_flag_settings.get_tags<std::string>( "blacklist" ) ) {
            new_region.overmap_feature_flag.blacklist.emplace( value );
        }
        for( const auto &value : feature_flag_settings.get_tags<std::string>( "whitelist" ) ) {
            new_region.overmap_feature_flag.whitelist.emplace( value );
        }
    } else {
        load_overmap_feature_flag_settings( jo, new_region.overmap_feature_flag, strict, false );
    }

    consume_bool_if_present( jo, "place_swamps" );
    consume_bool_if_present( jo, "place_roads" );
    consume_bool_if_present( jo, "place_railroads" );
    consume_bool_if_present( jo, "place_railroads_before_roads" );
    consume_bool_if_present( jo, "place_specials" );
    consume_bool_if_present( jo, "neighbor_connections" );

    if( jo.has_member( "rivers" ) && jo.has_null( "rivers" ) ) {
        new_region.river_scale = 0.0;
    }
    if( jo.has_member( "lakes" ) && jo.has_null( "lakes" ) ) {
        disable_lakes( new_region );
    }
    if( jo.has_member( "ocean" ) && jo.has_null( "ocean" ) ) {
        // BN has no separate ocean settings block here.
    }
    if( jo.has_member( "forests" ) && jo.has_null( "forests" ) ) {
        disable_forests( new_region );
    } else if( jo.has_string( "forests" ) ) {
        record_compat_region_reference( region_id, "forests", jo.get_string( "forests" ) );
    }
    if( jo.has_member( "forest_trails" ) && jo.has_null( "forest_trails" ) ) {
        new_region.forest_trail.chance = 0;
    }
    if( jo.has_member( "ravines" ) && jo.has_null( "ravines" ) ) {
        // BN has no ravines settings block here.
    }
    if( jo.has_member( "highways" ) && jo.has_null( "highways" ) ) {
        // BN has no highways settings block here.
    }

    const auto strict_overmap_forest = strict && !jo.has_member( "forests" );
    load_overmap_forest_settings( jo, new_region.overmap_forest, strict_overmap_forest, false );

    const auto strict_overmap_lake = strict && !jo.has_member( "lakes" );
    load_overmap_lake_settings( jo, new_region.overmap_lake, strict_overmap_lake, false );

    const auto strict_region_terrain_and_furniture = strict && !jo.has_string( "terrain_furniture" );
    load_region_terrain_and_furniture_settings( jo, new_region.region_terrain_and_furniture,
            strict_region_terrain_and_furniture, false );
    if( jo.has_string( "terrain_furniture" ) ) {
        record_compat_region_reference( region_id, "terrain_furniture",
                                        jo.get_string( "terrain_furniture" ) );
    }

    jo.read( "display_oter", new_region.display_oter );

    region_settings_map[new_region.id] = new_region;
}

auto finalize_compat_region_settings() -> void
{
    for( const auto &[region_id, city] : compat_region_settings_city_defs ) {
        if( auto region_iter = region_settings_map.find( region_id );
            region_iter != region_settings_map.end() ) {
            region_iter->second.city_spec = city.city;
        }
    }
    for( const auto &[region_id, forest] : compat_region_settings_forest_defs ) {
        if( auto region_iter = region_settings_map.find( region_id );
            region_iter != region_settings_map.end() ) {
            region_iter->second.overmap_forest = forest.forest;
        }
    }
    for( const auto &[region_id, refs] : compat_region_refs ) {
        auto region_iter = region_settings_map.find( region_id );
        if( region_iter == region_settings_map.end() ) {
            throw_unknown_compat_ref( "region_settings", region_id );
        }
        apply_compat_region_references( region_iter->second, refs );
    }
    for( const auto &[region_id, map_extras] : compat_region_settings_map_extras_defs ) {
        if( auto region_iter = region_settings_map.find( region_id );
            region_iter != region_settings_map.end() ) {
            apply_compat_map_extras_ref( region_iter->second, region_id );
        }
    }
    for( const auto &[region_id, forest_mapgen] : compat_region_settings_forest_mapgen_defs ) {
        if( auto region_iter = region_settings_map.find( region_id );
            region_iter != region_settings_map.end() ) {
            apply_compat_forest_mapgen_ref( region_iter->second, region_id );
        }
    }
    for( const auto &[region_id, terrain_furniture] : compat_region_settings_terrain_furniture_defs ) {
        if( auto region_iter = region_settings_map.find( region_id );
            region_iter != region_settings_map.end() ) {
            apply_compat_terrain_furniture_ref( region_iter->second, region_id );
        }
    }
}

void reset_region_settings()
{
    region_settings_map.clear();
    compat_weather_generators.clear();
    compat_map_extra_collection_defs.clear();
    compat_region_settings_map_extras_defs.clear();
    compat_region_terrain_furniture_defs.clear();
    compat_region_settings_terrain_furniture_defs.clear();
    compat_forest_biome_component_defs.clear();
    compat_forest_biome_mapgen_defs.clear();
    compat_region_settings_forest_mapgen_defs.clear();
    compat_region_settings_city_defs.clear();
    compat_region_settings_forest_defs.clear();
    compat_region_refs.clear();
}

/*
 Entry point for parsing "region_overlay" json objects.
 Will loop through and apply the overlay to each of the overlay's regions.
 */
void load_region_overlay( const JsonObject &jo )
{
    if( jo.has_array( "regions" ) ) {
        JsonArray regions = jo.get_array( "regions" );
        for( const std::string regionid : regions ) {
            if( regionid == "all" ) {
                if( regions.size() != 1 ) {
                    jo.throw_error( "regions: More than one region is not allowed when \"all\" is used" );
                }

                for( auto &itr : region_settings_map ) {
                    apply_region_overlay( jo, itr.second );
                }
            } else {
                auto itr = region_settings_map.find( regionid );
                if( itr == region_settings_map.end() ) {
                    jo.throw_error( "region: " + regionid + " not found in region_settings_map" );
                } else {
                    apply_region_overlay( jo, itr->second );
                }
            }
        }
    } else {
        jo.throw_error( "\"regions\" is required and must be an array" );
    }
}

void apply_region_overlay( const JsonObject &jo, regional_settings &region )
{
    read_default_oter( jo, region.default_oter );
    jo.read( "river_scale", region.river_scale );
    consume_bool_if_present( jo, "place_swamps" );
    consume_bool_if_present( jo, "place_roads" );
    consume_bool_if_present( jo, "place_railroads" );
    consume_bool_if_present( jo, "place_railroads_before_roads" );
    consume_bool_if_present( jo, "place_specials" );
    consume_bool_if_present( jo, "neighbor_connections" );
    if( jo.has_member( "rivers" ) && jo.has_null( "rivers" ) ) {
        region.river_scale = 0.0;
    }
    if( jo.has_member( "lakes" ) && jo.has_null( "lakes" ) ) {
        disable_lakes( region );
    }
    if( jo.has_member( "ocean" ) && jo.has_null( "ocean" ) ) {
        // BN has no separate ocean settings block here.
    }
    if( jo.has_member( "forests" ) && jo.has_null( "forests" ) ) {
        disable_forests( region );
    }
    if( jo.has_member( "forest_trails" ) && jo.has_null( "forest_trails" ) ) {
        region.forest_trail.chance = 0;
    }
    if( jo.has_member( "ravines" ) && jo.has_null( "ravines" ) ) {
        // BN has no ravines settings block here.
    }
    if( jo.has_member( "highways" ) && jo.has_null( "highways" ) ) {
        // BN has no highways settings block here.
    }

    if( jo.has_array( "default_groundcover" ) ) {
        region.default_groundcover_str.reset( new weighted_int_list<ter_str_id> );
        for( JsonArray inner : jo.get_array( "default_groundcover" ) ) {
            if( region.default_groundcover_str->add( ter_str_id( inner.get_string( 0 ) ),
                    inner.get_int( 1 ) ) == nullptr ) {
                jo.throw_error( "'default_groundcover' must be a weighted list: an array of pairs [ \"id\", weight ]" );
            }
        }
    }

    JsonObject fieldjo = jo.get_object( "field_coverage" );
    double tmpval = 0.0f;
    if( fieldjo.read( "percent_coverage", tmpval ) ) {
        region.field_coverage.mpercent_coverage = static_cast<int>( tmpval * 10000.0 );
    }

    fieldjo.read( "default_ter", region.field_coverage.default_ter_str );

    for( const JsonMember member : fieldjo.get_object( "other" ) ) {
        if( member.is_comment() ) {
            continue;
        }
        region.field_coverage.percent_str[member.name()] = member.get_float();
    }

    if( fieldjo.read( "boost_chance", tmpval ) ) {
        region.field_coverage.boost_chance = static_cast<int>( tmpval * 10000.0 );
    }
    if( fieldjo.read( "boosted_percent_coverage", tmpval ) ) {
        if( region.field_coverage.boost_chance > 0.0f && tmpval == 0.0f ) {
            fieldjo.throw_error( "boost_chance > 0 requires boosted_percent_coverage" );
        }

        region.field_coverage.boosted_mpercent_coverage = static_cast<int>( tmpval * 10000.0 );
    }

    if( fieldjo.read( "boosted_other_percent", tmpval ) ) {
        if( region.field_coverage.boost_chance > 0.0f && tmpval == 0.0f ) {
            fieldjo.throw_error( "boost_chance > 0 requires boosted_other_percent" );
        }

        region.field_coverage.boosted_other_mpercent = static_cast<int>( tmpval * 10000.0 );
    }

    for( const JsonMember member : fieldjo.get_object( "boosted_other" ) ) {
        if( member.is_comment() ) {
            continue;
        }
        region.field_coverage.boosted_percent_str[member.name()] = member.get_float();
    }

    if( region.field_coverage.boost_chance > 0.0f &&
        region.field_coverage.boosted_percent_str.empty() ) {
        fieldjo.throw_error( "boost_chance > 0 requires boosted_other { … }" );
    }

    load_forest_mapgen_settings( jo, region.forest_composition, false, true );
    if( jo.has_string( "forest_composition" ) ) {
        record_compat_region_reference( region.id, "forest_composition",
                                        jo.get_string( "forest_composition" ) );
    }

    load_forest_trail_settings( jo, region.forest_trail, false, true );

    if( jo.has_string( "map_extras" ) ) {
        record_compat_region_reference( region.id, "map_extras", jo.get_string( "map_extras" ) );
    }
    for( const JsonMember zone : jo.get_object( "map_extras" ) ) {
        if( zone.is_comment() ) {
            continue;
        }
        JsonObject zonejo = zone.get_object();

        int tmpval = 0;
        if( zonejo.read( "chance", tmpval ) ) {
            region.region_extras[zone.name()].chance = tmpval;
        }

        for( const JsonMember member : zonejo.get_object( "extras" ) ) {
            if( member.is_comment() ) {
                continue;
            }
            region.region_extras[zone.name()].values.add_or_replace( member.name(), member.get_int() );
        }
    }

    if( jo.has_string( "cities" ) ) {
        const auto cities_id = jo.get_string( "cities" );
        if( cities_id == "no_cities" ) {
            region.city_spec.city_size = 0;
            region.city_spec.city_spacing = 0;
        } else {
            record_compat_region_reference( region.id, "cities", cities_id );
        }
    }
    JsonObject cityjo = jo.get_object( "city" );

    cityjo.read( "city_size", region.city_spec.city_size );
    cityjo.read( "city_spacing", region.city_spec.city_spacing );
    cityjo.read( "shop_radius", region.city_spec.shop_radius );
    cityjo.read( "shop_sigma", region.city_spec.shop_sigma );
    cityjo.read( "park_radius", region.city_spec.park_radius );
    cityjo.read( "park_sigma", region.city_spec.park_sigma );

    const auto load_building_types = [&cityjo]( const std::string & type, building_bin & dest ) {
        for( const JsonMember member : cityjo.get_object( type ) ) {
            if( member.is_comment() ) {
                continue;
            }
            dest.add( overmap_special_id( member.name() ), member.get_int() );
        }
    };
    load_building_types( "houses", region.city_spec.houses );
    load_building_types( "urban_houses", region.city_spec.urban_houses );
    load_building_types( "shops", region.city_spec.shops );
    load_building_types( "urban_shops", region.city_spec.urban_shops );
    load_building_types( "parks", region.city_spec.parks );
    if( cityjo.has_member( "finales" ) ) {
        load_building_types( "finales", region.city_spec.finales );
    }
    if( jo.has_string( "weather" ) ) {
        record_compat_region_reference( region.id, "weather", jo.get_string( "weather" ) );
    }

    load_overmap_feature_flag_settings( jo, region.overmap_feature_flag, false, true );

    if( jo.has_string( "forests" ) ) {
        record_compat_region_reference( region.id, "forests", jo.get_string( "forests" ) );
    }
    load_overmap_forest_settings( jo, region.overmap_forest, false, true );

    load_overmap_lake_settings( jo, region.overmap_lake, false, true );

    if( jo.has_string( "terrain_furniture" ) ) {
        record_compat_region_reference( region.id, "terrain_furniture",
                                        jo.get_string( "terrain_furniture" ) );
    }
    load_region_terrain_and_furniture_settings( jo, region.region_terrain_and_furniture, false, true );

    jo.read( "display_oter", region.display_oter );
}

void groundcover_extra::finalize()   // FIXME: return bool for failure
{
    if( !default_ter_str.empty() ) {
        default_ter = ter_id( default_ter_str );
    }

    ter_furn_id tf_id;
    int wtotal = 0;
    int btotal = 0;

    for( std::map<std::string, double>::const_iterator it = percent_str.begin();
         it != percent_str.end(); ++it ) {
        tf_id.ter = t_null;
        tf_id.furn = f_null;
        if( it->second < 0.0001 ) {
            continue;
        }
        const ter_str_id tid( it->first );
        const furn_str_id fid( it->first );
        if( tid.is_valid() ) {
            tf_id.ter = tid.id();
        } else if( fid.is_valid() ) {
            tf_id.furn = fid.id();
        } else {
            debugmsg( "No clue what '%s' is!  No such terrain or furniture", it->first.c_str() );
            continue;
        }
        wtotal += static_cast<int>( it->second * 10000.0 );
        weightlist[ wtotal ] = tf_id;
    }

    for( std::map<std::string, double>::const_iterator it = boosted_percent_str.begin();
         it != boosted_percent_str.end(); ++it ) {
        tf_id.ter = t_null;
        tf_id.furn = f_null;
        if( it->second < 0.0001 ) {
            continue;
        }
        const ter_str_id tid( it->first );
        const furn_str_id fid( it->first );

        if( tid.is_valid() ) {
            tf_id.ter = tid.id();
        } else if( fid.is_valid() ) {
            tf_id.furn = fid.id();
        } else {
            debugmsg( "No clue what '%s' is!  No such terrain or furniture", it->first.c_str() );
            continue;
        }
        btotal += static_cast<int>( it->second * 10000.0 );
        boosted_weightlist[ btotal ] = tf_id;
    }

    if( wtotal > 1000000 ) {
        std::stringstream ss;
        for( auto it = percent_str.begin(); it != percent_str.end(); ++it ) {
            if( it != percent_str.begin() ) {
                ss << '+';
            }
            ss << it->second;
        }
        debugmsg( "plant coverage total (%s=%de-4) exceeds 100%%", ss.str(), wtotal );
    }
    if( btotal > 1000000 ) {
        std::stringstream ss;
        for( auto it = boosted_percent_str.begin(); it != boosted_percent_str.end(); ++it ) {
            if( it != boosted_percent_str.begin() ) {
                ss << '+';
            }
            ss << it->second;
        }
        debugmsg( "boosted plant coverage total (%s=%de-4) exceeds 100%%", ss.str(), btotal );
    }

    tf_id.furn = f_null;
    tf_id.ter = default_ter;
    weightlist[ 1000000 ] = tf_id;
    boosted_weightlist[ 1000000 ] = tf_id;

    percent_str.clear();
    boosted_percent_str.clear();
}

ter_furn_id groundcover_extra::pick( bool boosted ) const
{
    if( boosted ) {
        return boosted_weightlist.lower_bound( rng( 0, 1000000 ) )->second;
    }
    return weightlist.lower_bound( rng( 0, 1000000 ) )->second;
}

void forest_biome_component::finalize()
{
    for( const std::pair<const std::string, int> &pr : unfinalized_types ) {
        ter_furn_id tf_id;
        tf_id.ter = t_null;
        tf_id.furn = f_null;
        const ter_str_id tid( pr.first );
        const furn_str_id fid( pr.first );
        if( tid.is_valid() ) {
            tf_id.ter = tid.id();
        } else if( fid.is_valid() ) {
            tf_id.furn = fid.id();
        } else {
            continue;
        }
        types.add( tf_id, pr.second );
    }
}

void forest_biome_terrain_dependent_furniture::finalize()
{
    for( const std::pair<const std::string, int> &pr : unfinalized_furniture ) {
        const furn_str_id fid( pr.first );
        if( !fid.is_valid() ) {
            continue;
        }
        furniture.add( fid.id(), pr.second );
    }
}

ter_furn_id forest_biome::pick() const
{
    // Iterate through the biome components (which have already been put into sequence), roll for the
    // one_in chance that component contributes a feature, and if so pick that feature and return it.
    // If a given component does not roll as success, proceed to the next feature in sequence until
    // a feature is picked or none are picked, in which case an empty feature is returned.
    const ter_furn_id *result = nullptr;
    for( auto &pr : biome_components ) {
        if( one_in( pr.chance ) ) {
            result = pr.types.pick();
            break;
        }
    }

    if( result == nullptr ) {
        return ter_furn_id();
    }

    return *result;
}

void forest_biome::finalize()
{
    for( auto &pr : unfinalized_biome_components ) {
        pr.second.finalize();
        biome_components.push_back( pr.second );
    }

    std::sort( biome_components.begin(), biome_components.end(), []( const forest_biome_component & a,
    const forest_biome_component & b ) {
        return a.sequence < b.sequence;
    } );

    for( const std::pair<const std::string, int> &pr : unfinalized_groundcover ) {
        const ter_str_id tid( pr.first );
        if( !tid.is_valid() ) {
            continue;
        }
        groundcover.add( tid.id(), pr.second );
    }

    for( auto &pr : unfinalized_terrain_dependent_furniture ) {
        pr.second.finalize();
        const ter_str_id tid( pr.first );
        if( !tid.is_valid() ) {
            continue;
        }
        const auto t = tid.id();
        terrain_dependent_furniture[t] = pr.second;
    }
}

void forest_mapgen_settings::finalize()
{
    for( auto &pr : unfinalized_biomes ) {
        pr.second.finalize();
        const oter_id ot( pr.first );
        biomes[ot] = pr.second;
    }
}

void forest_trail_settings::finalize()
{
    for( const std::pair<const std::string, int> &pr : unfinalized_trail_terrain ) {
        const ter_str_id tid( pr.first );
        if( !tid.is_valid() ) {
            debugmsg( "Tried to add invalid terrain %s to forest_trail_settings trail_terrain.", tid.c_str() );
            continue;
        }
        trail_terrain.add( tid.id(), pr.second );
    }

    if( !trailheads.unfinalized_buildings.empty() ) {
        trailheads.finalize();
    }
}

void overmap_lake_settings::finalize()
{
    for( const std::string &oid : unfinalized_shore_extendable_overmap_terrain ) {
        const oter_str_id ot( oid );
        if( !ot.is_valid() ) {
            debugmsg( "Tried to add invalid overmap terrain %s to overmap_lake_settings shore_extendable_overmap_terrain.",
                      ot.c_str() );
            continue;
        }
        shore_extendable_overmap_terrain.emplace_back( ot.id() );
    }

    for( shore_extendable_overmap_terrain_alias &alias : shore_extendable_overmap_terrain_aliases ) {
        if( std::find( shore_extendable_overmap_terrain.begin(), shore_extendable_overmap_terrain.end(),
                       alias.alias ) == shore_extendable_overmap_terrain.end() ) {
            debugmsg( " %s was referenced as an alias in overmap_lake_settings shore_extendable_overmap_terrain_alises, but the value is not present in the shore_extendable_overmap_terrain.",
                      alias.alias.c_str() );
            continue;
        }
    }
}

void region_terrain_and_furniture_settings::finalize()
{
    for( auto const &template_pr : unfinalized_terrain ) {
        const ter_str_id template_tid( template_pr.first );
        if( !template_tid.is_valid() ) {
            debugmsg( "Tried to add invalid regional template terrain %s to region_terrain_and_furniture terrain.",
                      template_tid.c_str() );
            continue;
        }
        for( auto const &actual_pr : template_pr.second ) {
            const ter_str_id tid( actual_pr.first );
            if( !tid.is_valid() ) {
                debugmsg( "Tried to add invalid regional terrain %s to region_terrain_and_furniture terrain template %s.",
                          tid.c_str(), template_tid.c_str() );
                continue;
            }
            terrain[template_tid.id()].add( tid.id(), actual_pr.second );
        }
    }

    for( auto const &template_pr : unfinalized_furniture ) {
        const furn_str_id template_fid( template_pr.first );
        if( !template_fid.is_valid() ) {
            debugmsg( "Tried to add invalid regional template furniture %s to region_terrain_and_furniture furniture.",
                      template_fid.c_str() );
            continue;
        }
        for( auto const &actual_pr : template_pr.second ) {
            const furn_str_id fid( actual_pr.first );
            if( !fid.is_valid() ) {
                debugmsg( "Tried to add invalid regional furniture %s to region_terrain_and_furniture furniture template %s.",
                          fid.c_str(), template_fid.c_str() );
                continue;
            }
            furniture[template_fid.id()].add( fid.id(), actual_pr.second );
        }
    }
}

ter_id region_terrain_and_furniture_settings::resolve( const ter_id &tid ) const
{
    ter_id result = tid;
    auto region_list = terrain.find( result );
    while( region_list != terrain.end() ) {
        result = *region_list->second.pick();
        region_list = terrain.find( result );
    }
    return result;
}

furn_id region_terrain_and_furniture_settings::resolve( const furn_id &fid ) const
{
    furn_id result = fid;
    auto region_list = furniture.find( result );
    while( region_list != furniture.end() ) {
        result = *region_list->second.pick();
        region_list = furniture.find( result );
    }
    return result;
}

void regional_settings::finalize()
{
    if( default_groundcover_str != nullptr ) {
        for( const auto &pr : *default_groundcover_str ) {
            default_groundcover.add( pr.obj.id(), pr.weight );
        }

        field_coverage.finalize();
        default_groundcover_str.reset();
        city_spec.finalize();
        forest_composition.finalize();
        forest_trail.finalize();
        overmap_lake.finalize();
        region_terrain_and_furniture.finalize();
        get_options().add_value( "DEFAULT_REGION", id, no_translation( id ) );
    }
}

overmap_special_id city_settings::pick_house() const
{
    return houses.pick()->id;
}

overmap_special_id city_settings::pick_urban_house() const
{
    return urban_houses.pick()->id;
}

overmap_special_id city_settings::pick_shop() const
{
    return shops.pick()->id;
}

overmap_special_id city_settings::pick_urban_shop() const
{
    return urban_shops.pick()->id;
}

overmap_special_id city_settings::pick_park() const
{
    return parks.pick()->id;
}

overmap_special_id city_settings::pick_finale() const
{
    return finales.pick()->id;
}

void city_settings::finalize()
{
    if( !houses.unfinalized_buildings.empty() ) {
        houses.finalize();
    }
    if( !urban_houses.unfinalized_buildings.empty() ) {
        urban_houses.finalize();
    }
    if( !shops.unfinalized_buildings.empty() ) {
        shops.finalize();
    }
    if( !urban_shops.unfinalized_buildings.empty() ) {
        urban_shops.finalize();
    }
    if( !parks.unfinalized_buildings.empty() ) {
        parks.finalize();
    }
    if( !finales.unfinalized_buildings.empty() ) {
        finales.finalize();
    }
}

void building_bin::add( const overmap_special_id &building, int weight )
{
    if( finalized ) {
        debugmsg( "Tried to add special %s to a finalized building bin", building.c_str() );
        return;
    }

    unfinalized_buildings[ building ] = weight;
}

overmap_special_id building_bin::pick() const
{
    if( !finalized ) {
        debugmsg( "Tried to pick a special out of a non-finalized bin" );
        return overmap_special_id( "null" );
    }

    const auto *result = buildings.pick();
    if( !result ) {
        return overmap_special_id( "null" );
    }
    return *result;
}

void building_bin::clear()
{
    finalized = false;
    buildings.clear();
    unfinalized_buildings.clear();
    all.clear();
}

void building_bin::finalize()
{
    if( finalized ) {
        debugmsg( "Tried to finalize a finalized bin (that's a code-side error which can't be fixed with jsons)" );
        return;
    }
    // Empty bins are valid — pick() returns null_special, causing no building to be placed.

    for( const std::pair<const overmap_special_id, int> &pr : unfinalized_buildings ) {
        overmap_special_id current_id = pr.first;
        if( !current_id.is_valid() ) {
            // First, try to convert oter to special
            oter_type_str_id converted_id( pr.first.str() );
            if( !converted_id.is_valid() ) {
                debugmsg( "Tried to add city building %s, but it is neither a special nor a terrain type",
                          pr.first.c_str() );
                continue;
            } else {
                all.emplace_back( pr.first.str() );
            }
            current_id = overmap_specials::create_building_from( converted_id );
        }
        buildings.add( current_id, pr.second );
    }

    finalized = true;
}

void check_regional_settings()
{
    for( auto const& [region_id, region] : region_settings_map ) {
        consistency_report rep;

        for( auto const& [extras_group, extras] : region.region_extras ) {
            for( auto const& [extra_id, extra_weight] : extras.values ) {
                string_id<map_extra> id( extra_id );
                if( !id.is_valid() ) {
                    rep.warn( "defines unknown map extra '%s'", id );
                }
            }
        }

        if( !rep.is_empty() ) {
            debugmsg( rep.format( "region_settings", region_id ) );
        }
    }
}
