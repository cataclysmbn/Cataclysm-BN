#include "regional_settings.h"

#include <algorithm>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_set>
#include <utility>

#include "consistency_report.h"
#include "debug.h"
#include "enum_conversions.h"
#include "int_id.h"
#include "json.h"
#include "map_extras.h"
#include "options.h"
#include "overmap_special.h"
#include "rng.h"
#include "assign.h"
#include "string_formatter.h"
#include "string_id.h"
#include "translations.h"
#include "weather_type.h"

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

struct compat_region_settings_forest_mapgen {
    std::optional<std::string> copy_from;
    std::vector<std::string> biomes;
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
auto compat_region_settings_forest_mapgen_defs = std::unordered_map<std::string,
compat_region_settings_forest_mapgen> {};

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

auto make_weather_type_list( const weather_generator &base, const JsonObject &jo ) ->
std::vector<weather_type_id>
{
    if( jo.has_array( "weather_types" ) ) {
        auto result = std::vector<weather_type_id> {};
        jo.read( "weather_types", result );
        return result;
    }

    auto result = base.weather_types;
    if( result.empty() ) {
        for( const auto &weather : weather_types::get_all() ) {
            result.emplace_back( weather.id );
        }
    }

    if( jo.has_array( "weather_black_list" ) ) {
        const auto blocked = jo.get_tags<std::string>( "weather_black_list" );
        auto filtered = std::vector<weather_type_id> {};
        filtered.reserve( result.size() );
        for( const auto &weather_id : result ) {
            if( !blocked.contains( weather_id.str() ) ) {
                filtered.push_back( weather_id );
            }
        }
        result = std::move( filtered );
    }

    if( jo.has_array( "weather_white_list" ) ) {
        const auto allowed = jo.get_tags<std::string>( "weather_white_list" );
        auto filtered = std::vector<weather_type_id> {};
        filtered.reserve( result.size() );
        for( const auto &weather_id : result ) {
            if( allowed.contains( weather_id.str() ) || weather_id == weather_type_id( "clear" ) ) {
                filtered.push_back( weather_id );
            }
        }
        result = std::move( filtered );
    }

    return result;
}

auto load_compat_weather_generator_impl( const JsonObject &jo ) -> weather_generator
{
    auto result = get_default_weather_generator();
    if( jo.has_string( "copy-from" ) ) {
        const auto copy_from = jo.get_string( "copy-from" );
        if( copy_from == "default" ) {
            result = get_default_weather_generator();
        } else if( const auto iter = compat_weather_generators.find( copy_from );
                   iter != compat_weather_generators.end() ) {
            result = iter->second;
        } else {
            jo.throw_error( string_format( "unknown weather_generator copy-from id '%s'", copy_from ) );
        }
    }

    const auto base_temp = jo.get_float( "base_temperature", 0.0 );
    const auto season_offsets = std::array<int, NUM_SEASONS> { 0, 10, 0, -15 };
    const auto humidity_keys = std::array<std::string, NUM_SEASONS> {
        "spring_humidity_manual_mod",
        "summer_humidity_manual_mod",
        "autumn_humidity_manual_mod",
        "winter_humidity_manual_mod"
    };
    const auto legacy_temp_keys = std::array<std::string, NUM_SEASONS> {
        "spring_temp_manual_mod",
        "summer_temp_manual_mod",
        "autumn_temp_manual_mod",
        "winter_temp_manual_mod"
    };
    for( int i = 0; i < NUM_SEASONS; ++i ) {
        if( jo.has_member( "base_temperature" ) || jo.has_member( legacy_temp_keys[i] ) ) {
            result.season_stats[i].average_temperature = units::from_celsius(
                        base_temp + jo.get_int( legacy_temp_keys[i], 0 ) + season_offsets[i] );
        }
        assign( jo, humidity_keys[i], result.season_stats[i].humidity_mod );
    }

    result.base_humidity = jo.get_float( "base_humidity", result.base_humidity );
    result.base_pressure = jo.get_float( "base_pressure", result.base_pressure );
    result.base_acid = jo.get_float( "base_acid", result.base_acid );
    result.base_wind = jo.get_float( "base_wind", result.base_wind );
    result.base_wind_distrib_peaks = jo.get_int( "base_wind_distrib_peaks",
                                     result.base_wind_distrib_peaks );
    result.base_wind_season_variation = jo.get_int( "base_wind_season_variation",
                                        result.base_wind_season_variation );
    result.weather_types = make_weather_type_list( result, jo );
    if( result.weather_types.empty() ) {
        jo.throw_error( "expected at least 1 weather type" );
    }
    return result;
}

auto apply_default_region_extras( regional_settings &region ) -> void
{
    if( const auto *default_region = get_default_region_settings() ) {
        region.region_extras = default_region->region_extras;
    }
}

auto apply_default_region_terrain_and_furniture( regional_settings &region ) -> void
{
    if( const auto *default_region = get_default_region_settings() ) {
        region.region_terrain_and_furniture = default_region->region_terrain_and_furniture;
    }
}

auto apply_compat_map_extra_collection( map_extras &target,
                                        const compat_map_extra_collection &collection ) -> void
{
    target.chance = std::max( target.chance, collection.chance );
    for( const auto &entry : collection.values ) {
        target.values.add_or_replace( entry.obj, entry.weight );
    }
}

auto apply_region_settings_map_extras_compat( regional_settings &region, const std::string &id,
        std::unordered_set<std::string> &visited ) -> void
{
    if( !visited.insert( id ).second ) {
        return;
    }
    const auto iter = compat_region_settings_map_extras_defs.find( id );
    if( iter == compat_region_settings_map_extras_defs.end() ) {
        return;
    }
    if( iter->second.copy_from.has_value() ) {
        if( *iter->second.copy_from == "default" ) {
            apply_default_region_extras( region );
        } else {
            apply_region_settings_map_extras_compat( region, *iter->second.copy_from, visited );
        }
    }
    for( const auto &collection_id : iter->second.extras ) {
        if( const auto collection_iter = compat_map_extra_collection_defs.find( collection_id );
            collection_iter != compat_map_extra_collection_defs.end() ) {
            apply_compat_map_extra_collection( region.region_extras[collection_id], collection_iter->second );
        }
    }
}

auto apply_compat_region_terrain_furniture( region_terrain_and_furniture_settings &target,
        const compat_region_terrain_furniture &definition ) -> void
{
    if( definition.replaced_terrain_id.has_value() ) {
        auto &terrain_list = target.terrain[ter_id( *definition.replaced_terrain_id )];
        terrain_list.clear();
        for( const auto &entry : definition.terrain ) {
            terrain_list.add_or_replace( entry.obj, entry.weight );
        }
    }
    if( definition.replaced_furniture_id.has_value() ) {
        auto &furniture_list = target.furniture[furn_id( *definition.replaced_furniture_id )];
        furniture_list.clear();
        for( const auto &entry : definition.furniture ) {
            furniture_list.add_or_replace( entry.obj, entry.weight );
        }
    }
}

auto apply_region_settings_terrain_furniture_compat( regional_settings &region,
        const std::string &id, std::unordered_set<std::string> &visited ) -> void
{
    if( !visited.insert( id ).second ) {
        return;
    }
    const auto iter = compat_region_settings_terrain_furniture_defs.find( id );
    if( iter == compat_region_settings_terrain_furniture_defs.end() ) {
        return;
    }
    if( iter->second.copy_from.has_value() ) {
        if( *iter->second.copy_from == "default" ) {
            apply_default_region_terrain_and_furniture( region );
        } else {
            apply_region_settings_terrain_furniture_compat( region, *iter->second.copy_from, visited );
        }
    }
    for( const auto &ter_furn_id : iter->second.ter_furn ) {
        if( const auto compat_iter = compat_region_terrain_furniture_defs.find( ter_furn_id );
            compat_iter != compat_region_terrain_furniture_defs.end() ) {
            apply_compat_region_terrain_furniture( region.region_terrain_and_furniture, compat_iter->second );
        }
    }
}

auto disable_lakes( regional_settings &region ) -> void
{
    region.overmap_lake.noise_threshold_lake = 1.1;
    region.overmap_lake.lake_size_min = std::numeric_limits<int>::max();
}

auto disable_forests( regional_settings &region ) -> void
{
    region.place_forests = false;
    region.place_forest_trails = false;
    region.overmap_forest.noise_threshold_forest = 1.1;
    region.overmap_forest.noise_threshold_forest_thick = 1.1;
    region.overmap_forest.noise_threshold_swamp_adjacent_water = 1.1;
    region.overmap_forest.noise_threshold_swamp_isolated = 1.1;
}

auto disable_cities( regional_settings &region ) -> void
{
    region.place_cities = false;
    region.place_roads = false;
    region.city_spec.houses.clear();
    region.city_spec.shops.clear();
    region.city_spec.parks.clear();
    region.city_spec.finales.clear();
}

} // namespace

auto load_weather_generator_compat( const JsonObject &jo ) -> void
{
    compat_weather_generators[jo.get_string( "id" )] = load_compat_weather_generator_impl( jo );
}

auto load_map_extra_collection_compat( const JsonObject &jo ) -> void
{
    auto collection = compat_map_extra_collection {};
    if( jo.has_string( "copy-from" ) ) {
        const auto copy_from = jo.get_string( "copy-from" );
        if( const auto iter = compat_map_extra_collection_defs.find( copy_from );
            iter != compat_map_extra_collection_defs.end() ) {
            collection = iter->second;
        }
        collection.copy_from = copy_from;
    }
    collection.chance = jo.get_int( "chance", collection.chance );
    if( jo.has_array( "extras" ) ) {
        collection.values.clear();
        for( JsonArray entry : jo.get_array( "extras" ) ) {
            collection.values.add_or_replace( entry.get_string( 0 ), entry.get_int( 1 ) );
        }
    }
    compat_map_extra_collection_defs[jo.get_string( "id" )] = collection;
}

auto load_region_settings_map_extras_compat( const JsonObject &jo ) -> void
{
    auto definition = compat_region_settings_map_extras {};
    if( jo.has_string( "copy-from" ) ) {
        definition.copy_from = jo.get_string( "copy-from" );
    }
    if( jo.has_array( "extras" ) ) {
        const auto extras = jo.get_tags<std::string>( "extras" );
        definition.extras.assign( extras.begin(), extras.end() );
    }
    compat_region_settings_map_extras_defs[jo.get_string( "id" )] = definition;
}

auto load_region_terrain_furniture_compat( const JsonObject &jo ) -> void
{
    auto definition = compat_region_terrain_furniture {};
    if( jo.has_string( "copy-from" ) ) {
        definition.copy_from = jo.get_string( "copy-from" );
    }
    if( jo.has_string( "ter_id" ) ) {
        definition.replaced_terrain_id = jo.get_string( "ter_id" );
    }
    if( jo.has_string( "furn_id" ) ) {
        definition.replaced_furniture_id = jo.get_string( "furn_id" );
    }
    if( jo.has_array( "replace_with_terrain" ) ) {
        definition.terrain.clear();
        for( JsonArray entry : jo.get_array( "replace_with_terrain" ) ) {
            definition.terrain.add_or_replace( ter_id( entry.get_string( 0 ) ), entry.get_int( 1 ) );
        }
    }
    if( jo.has_array( "replace_with_furniture" ) ) {
        definition.furniture.clear();
        for( JsonArray entry : jo.get_array( "replace_with_furniture" ) ) {
            definition.furniture.add_or_replace( furn_id( entry.get_string( 0 ) ), entry.get_int( 1 ) );
        }
    }
    compat_region_terrain_furniture_defs[jo.get_string( "id" )] = definition;
}

auto load_region_settings_terrain_furniture_compat( const JsonObject &jo ) -> void
{
    auto definition = compat_region_settings_terrain_furniture {};
    if( jo.has_string( "copy-from" ) ) {
        definition.copy_from = jo.get_string( "copy-from" );
    }
    if( jo.has_array( "ter_furn" ) ) {
        const auto ter_furn = jo.get_tags<std::string>( "ter_furn" );
        definition.ter_furn.assign( ter_furn.begin(), ter_furn.end() );
    }
    compat_region_settings_terrain_furniture_defs[jo.get_string( "id" )] = definition;
}

auto load_region_settings_forest_mapgen_compat( const JsonObject &jo ) -> void
{
    auto definition = compat_region_settings_forest_mapgen {};
    if( jo.has_string( "copy-from" ) ) {
        definition.copy_from = jo.get_string( "copy-from" );
    }
    if( jo.has_array( "biomes" ) ) {
        const auto biomes = jo.get_tags<std::string>( "biomes" );
        definition.biomes.assign( biomes.begin(), biomes.end() );
    }
    compat_region_settings_forest_mapgen_defs[jo.get_string( "id" )] = definition;
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
    if( jo.has_string( "copy-from" ) ) {
        const auto copy_from = jo.get_string( "copy-from" );
        if( const auto iter = region_settings_map.find( copy_from ); iter != region_settings_map.end() ) {
            new_region = iter->second;
            new_region.id = jo.get_string( "id" );
        } else {
            jo.throw_error( string_format( "unknown region_settings copy-from id '%s'", copy_from ) );
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

    load_forest_mapgen_settings( jo, new_region.forest_composition, strict, false );

    load_forest_trail_settings( jo, new_region.forest_trail, strict, false );

    if( !jo.has_object( "map_extras" ) ) {
        if( strict ) {
            jo.throw_error( "\"map_extras\": { … } required for default" );
        }
    } else {
        for( const JsonMember zone : jo.get_object( "map_extras" ) ) {
            if( zone.is_comment() ) {
                continue;
            }
            JsonObject zjo = zone.get_object();
            map_extras extras( 0 );

            if( !zjo.read( "chance", extras.chance ) && strict ) {
                zjo.throw_error( "chance required for default" );
            }

            if( !zjo.has_object( "extras" ) ) {
                if( strict ) {
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
        new_region.region_map_extras_id = jo.get_string( "map_extras" );
    }

    if( jo.has_string( "cities" ) ) {
        new_region.city_settings_id = jo.get_string( "cities" );
        if( *new_region.city_settings_id == "no_cities" ) {
            disable_cities( new_region );
        }
    } else if( jo.has_member( "cities" ) && jo.has_null( "cities" ) ) {
        disable_cities( new_region );
    } else if( !jo.has_object( "city" ) ) {
        if( strict ) {
            jo.throw_error( "\"city\": { … } required for default" );
        }
    } else {
        JsonObject cjo = jo.get_object( "city" );
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
        load_building_types( "shops", new_region.city_spec.shops );
        load_building_types( "parks", new_region.city_spec.parks );
        if( cjo.has_member( "finales" ) ) {
            load_building_types( "finales", new_region.city_spec.finales );
        }
    }

    if( jo.has_string( "weather" ) ) {
        new_region.weather_generator_id = jo.get_string( "weather" );
    } else if( !jo.has_object( "weather" ) ) {
        if( strict ) {
            jo.throw_error( "\"weather\": { … } required for default" );
        }
    } else {
        JsonObject wjo = jo.get_object( "weather" );
        new_region.weather = weather_generator::load( wjo );
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

    if( jo.has_bool( "place_roads" ) ) {
        new_region.place_roads = jo.get_bool( "place_roads" );
    }
    if( jo.has_bool( "neighbor_connections" ) ) {
        new_region.neighbor_connections = jo.get_bool( "neighbor_connections" );
    }

    if( jo.has_member( "rivers" ) && jo.has_null( "rivers" ) ) {
        new_region.river_scale = 0.0;
    }
    if( jo.has_member( "lakes" ) && jo.has_null( "lakes" ) ) {
        disable_lakes( new_region );
    }
    if( jo.has_member( "forests" ) && jo.has_null( "forests" ) ) {
        disable_forests( new_region );
    }
    if( jo.has_member( "forest_trails" ) && jo.has_null( "forest_trails" ) ) {
        new_region.place_forest_trails = false;
    }

    load_overmap_forest_settings( jo, new_region.overmap_forest, strict, false );

    load_overmap_lake_settings( jo, new_region.overmap_lake, strict, false );

    load_region_terrain_and_furniture_settings( jo, new_region.region_terrain_and_furniture, strict,
            false );

    if( jo.has_string( "terrain_furniture" ) ) {
        new_region.region_terrain_and_furniture_id = jo.get_string( "terrain_furniture" );
    }
    if( jo.has_string( "forest_composition" ) ) {
        new_region.forest_composition_id = jo.get_string( "forest_composition" );
    }

    region_settings_map[new_region.id] = new_region;
}

void reset_region_settings()
{
    region_settings_map.clear();
    compat_weather_generators.clear();
    compat_map_extra_collection_defs.clear();
    compat_region_settings_map_extras_defs.clear();
    compat_region_terrain_furniture_defs.clear();
    compat_region_settings_terrain_furniture_defs.clear();
    compat_region_settings_forest_mapgen_defs.clear();
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
    if( jo.has_bool( "place_roads" ) ) {
        region.place_roads = jo.get_bool( "place_roads" );
    }
    if( jo.has_bool( "neighbor_connections" ) ) {
        region.neighbor_connections = jo.get_bool( "neighbor_connections" );
    }
    if( jo.has_member( "rivers" ) && jo.has_null( "rivers" ) ) {
        region.river_scale = 0.0;
    }
    if( jo.has_member( "lakes" ) && jo.has_null( "lakes" ) ) {
        disable_lakes( region );
    }
    if( jo.has_member( "forests" ) && jo.has_null( "forests" ) ) {
        disable_forests( region );
    }
    if( jo.has_member( "forest_trails" ) && jo.has_null( "forest_trails" ) ) {
        region.place_forest_trails = false;
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

    load_forest_trail_settings( jo, region.forest_trail, false, true );

    if( jo.has_object( "map_extras" ) ) {
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
    } else if( jo.has_string( "map_extras" ) ) {
        region.region_map_extras_id = jo.get_string( "map_extras" );
    }

    if( jo.has_string( "cities" ) ) {
        region.city_settings_id = jo.get_string( "cities" );
        if( *region.city_settings_id == "no_cities" ) {
            disable_cities( region );
        }
    } else if( jo.has_member( "cities" ) && jo.has_null( "cities" ) ) {
        disable_cities( region );
    } else if( jo.has_object( "city" ) ) {
        JsonObject cityjo = jo.get_object( "city" );

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
        load_building_types( "shops", region.city_spec.shops );
        load_building_types( "parks", region.city_spec.parks );
        if( cityjo.has_member( "finales" ) ) {
            load_building_types( "finales", region.city_spec.finales );
        }
    }

    if( jo.has_object( "feature_flag_settings" ) ) {
        auto feature_flag_settings = jo.get_object( "feature_flag_settings" );
        read_and_set_or_throw<bool>( feature_flag_settings, "clear_blacklist",
                                     region.overmap_feature_flag.clear_blacklist, false );
        read_and_set_or_throw<bool>( feature_flag_settings, "clear_whitelist",
                                     region.overmap_feature_flag.clear_whitelist, false );
        if( region.overmap_feature_flag.clear_blacklist ) {
            region.overmap_feature_flag.blacklist.clear();
        }
        if( region.overmap_feature_flag.clear_whitelist ) {
            region.overmap_feature_flag.whitelist.clear();
        }
        for( const auto &value : feature_flag_settings.get_tags<std::string>( "blacklist" ) ) {
            region.overmap_feature_flag.blacklist.emplace( value );
        }
        for( const auto &value : feature_flag_settings.get_tags<std::string>( "whitelist" ) ) {
            region.overmap_feature_flag.whitelist.emplace( value );
        }
    } else {
        load_overmap_feature_flag_settings( jo, region.overmap_feature_flag, false, true );
    }

    load_overmap_forest_settings( jo, region.overmap_forest, false, true );

    load_overmap_lake_settings( jo, region.overmap_lake, false, true );

    load_region_terrain_and_furniture_settings( jo, region.region_terrain_and_furniture, false, true );
    if( jo.has_string( "weather" ) ) {
        region.weather_generator_id = jo.get_string( "weather" );
    }
    if( jo.has_string( "terrain_furniture" ) ) {
        region.region_terrain_and_furniture_id = jo.get_string( "terrain_furniture" );
    }
    if( jo.has_string( "forest_composition" ) ) {
        region.forest_composition_id = jo.get_string( "forest_composition" );
    }
}

void groundcover_extra::finalize()   // FIXME: return bool for failure
{
    default_ter = ter_id( default_ter_str );

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
        const ter_id t( pr.first );
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

    trailheads.finalize();
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
    if( weather_generator_id.has_value() ) {
        if( *weather_generator_id == "default" ) {
            weather = get_default_weather_generator();
        } else if( const auto iter = compat_weather_generators.find( *weather_generator_id );
                   iter != compat_weather_generators.end() ) {
            weather = iter->second;
        }
    }

    if( region_map_extras_id.has_value() ) {
        auto visited = std::unordered_set<std::string> {};
        apply_region_settings_map_extras_compat( *this, *region_map_extras_id, visited );
    }

    if( region_terrain_and_furniture_id.has_value() ) {
        auto visited = std::unordered_set<std::string> {};
        apply_region_settings_terrain_furniture_compat( *this, *region_terrain_and_furniture_id, visited );
    }

    if( default_groundcover_str != nullptr ) {
        for( const auto &pr : *default_groundcover_str ) {
            default_groundcover.add( pr.obj.id(), pr.weight );
        }
        default_groundcover_str.reset();
    }

    field_coverage.finalize();
    city_spec.finalize();
    forest_composition.finalize();
    forest_trail.finalize();
    overmap_lake.finalize();
    region_terrain_and_furniture.finalize();
    get_options().add_value( "DEFAULT_REGION", id, no_translation( id ) );
}

overmap_special_id city_settings::pick_house() const
{
    return houses.pick()->id;
}

overmap_special_id city_settings::pick_shop() const
{
    return shops.pick()->id;
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
    houses.finalize();
    shops.finalize();
    parks.finalize();
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
        overmap_special_id null_special( "null" );
        return null_special;
    }

    return *buildings.pick();
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
    if( unfinalized_buildings.empty() ) {
        debugmsg( "There must be at least one entry in this building bin." );
        return;
    }

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
