#include "init.h"

#include <cassert>
#include <cstddef>
#include <exception>
#include <fstream>
#include <iterator>
#include <memory>
#include <sstream> // for throwing errors
#include <stdexcept>
#include <string>
#include <vector>
#include <ranges>

#include "achievement.h"
#include "activity_type.h"
#include "ammo.h"
#include "ammo_effect.h"
#include "anatomy.h"
#include "ascii_art.h"
#include "artifact.h"
#include "behavior.h"
#include "bionics.h"
#include "bodypart.h"
#include "catalua.h"
#include "cata_utility.h"
#include "catalua_impl.h"
#include "clothing_mod.h"
#include "clzones.h"
#include "construction.h"
#include "construction_category.h"
#include "construction_group.h"
#include "crafting_gui.h"
#include "creature.h"
#include "cursesdef.h"
#include "debug.h"
#include "dependency_tree.h"
#include "dialogue.h"
#include "disease.h"
#include "effect.h"
#include "emit.h"
#include "event_statistics.h"
#include "faction.h"
#include "fault.h"
#include "field_type.h"
#include "filesystem.h"
#include "fstream_utils.h"
#include "flag.h"
#include "flag_trait.h"
#include "gates.h"
#include "harvest.h"
#include "hunting_data.h"
#include "item_action.h"
#include "item_category.h"
#include "item_factory.h"
#include "json.h"
#include "language.h"
#include "loading_ui.h"
#include "lru_cache.h"
#include "magic.h"
#include "magic_enchantment.h"
#include "magic_ter_furn_transform.h"
#include "map_extras.h"
#include "mapbuffer.h"
#include "mapdata.h"
#include "mapgen.h"
#include "martialarts.h"
#include "material.h"
#include "mission.h"
#include "mod_manager.h"
#include "monfaction.h"
#include "mongroup.h"
#include "monstergenerator.h"
#include "morale_types.h"
#include "mutation_data.h"
#include "mutation.h"
#include "npc.h"
#include "npc_class.h"
#include "omdata.h"
#include "overlay_ordering.h"
#include "overmap.h"
#include "overmapbuffer.h"
#include "overmap_connection.h"
#include "overmap_location.h"
#include "overmap_special.h"
#include "profession.h"
#include "recipe_dictionary.h"
#include "recipe_groups.h"
#include "regional_settings.h"
#include "requirements.h"
#include "rotatable_symbols.h"
#include "scenario.h"
#include "scent_map.h"
#include "sdltiles.h"
#include "skill.h"
#include "skill_boost.h"
#include "sounds.h"
#include "speech.h"
#include "start_location.h"
#include "string_formatter.h"
#include "text_snippets.h"
#include "translations.h"
#include "trap.h"
#include "type_id.h"
#include "veh_type.h"
#include "vehicle_group.h"
#include "vitamin.h"
#include "weather.h"
#include "weather_type.h"
#include "worldfactory.h"

#if defined(TILES)
#  include "mod_tileset.h"
#endif

struct DynamicDataLoader::cached_streams {
    lru_cache<std::string, shared_ptr_fast<std::istringstream>> cache;
};

DynamicDataLoader::DynamicDataLoader()
{
    initialize();
}

DynamicDataLoader::~DynamicDataLoader() = default;

DynamicDataLoader &DynamicDataLoader::get_instance()
{
    static DynamicDataLoader theDynamicDataLoader;
    return theDynamicDataLoader;
}

void DynamicDataLoader::load_object( const JsonObject &jo, const std::string &src,
                                     const std::string &base_path,
                                     const std::string &full_path )
{
    const std::string type = jo.get_string( "type" );
    const t_type_function_map::iterator it = type_function_map.find( type );
    if( it == type_function_map.end() ) {
        jo.throw_error( "unrecognized JSON object", "type" );
    }
    it->second( jo, src, base_path, full_path );
}

shared_ptr_fast<std::istream> DynamicDataLoader::get_cached_stream( const std::string &path )
{
    assert( !finalized && "Cannot open data file after finalization." );
    assert( stream_cache && "Stream cache is only available during finalization" );
    shared_ptr_fast<std::istringstream> cached = stream_cache->cache.get( path, nullptr );
    // Create a new stream if the file is not opened yet, or if some code is still
    // using the previous stream (in such case, `cached` and `stream_cache` have
    // two references to the stream, hence the test for > 2).
    if( !cached ) {
        cached = make_shared_fast<std::istringstream>( read_entire_file( path ) );
    } else if( cached.use_count() > 2 ) {
        cached = make_shared_fast<std::istringstream>( cached->str() );
    }
    stream_cache->cache.insert( 8, path, cached );
    return cached;
}

void DynamicDataLoader::load_deferred( deferred_json &data )
{
    while( !data.empty() ) {
        const size_t n = data.size();
        for( size_t idx = 0; idx != n; ++idx ) {
            auto it = data.begin();
            std::advance( it, idx );
            if( !it->first.path ) {
                debugmsg( "JSON source location has null path, data may load incorrectly" );
            } else {
                try {
                    shared_ptr_fast<std::istream> stream = get_cached_stream( *it->first.path );
                    JsonIn jsin( *stream, it->first );
                    JsonObject jo = jsin.get_object();
                    load_object( jo, it->second );
                } catch( const JsonError &err ) {
                    debugmsg( "(json-error)\n%s", err.what() );
                }
            }
            inp_mngr.pump_events();
        }
        auto it = data.begin();
        std::advance( it, n );
        data.erase( data.begin(), it );
        if( data.size() == n ) {
            for( const auto &elem : data ) {
                if( !elem.first.path ) {
                    debugmsg( "JSON source location has null path when reporting circular dependency" );
                } else {
                    try {
                        throw_error_at_json_loc( elem.first,
                                                 "JSON contains circular dependency, this object is discarded" );
                    } catch( const JsonError &err ) {
                        debugmsg( "(json-error)\n%s", err.what() );
                    }
                }
                inp_mngr.pump_events();
            }
            data.clear();
            return; // made no progress on this cycle so abort
        }
    }
}

static void load_ignored_type( const JsonObject &jo )
{
    // This does nothing!
    // This function is used for types that are to be ignored
    // (for example for testing or for unimplemented types)
    jo.allow_omitted_members();
}

void DynamicDataLoader::add( const std::string &type,
                             std::function<void( const JsonObject &, const std::string &, const std::string &, const std::string & )>
                             f )
{
    const auto pair = type_function_map.emplace( type, f );
    if( !pair.second ) {
        debugmsg( "tried to insert a second handler for type %s into the DynamicDataLoader", type.c_str() );
    }
}

void DynamicDataLoader::add( const std::string &type,
                             const std::function<void( const JsonObject &, const std::string & )> &f )
{
    const auto pair = type_function_map.emplace( type, [f]( const JsonObject & obj,
                      const std::string & src,
    const std::string &, const std::string & ) {
        f( obj, src );
    } );
    if( !pair.second ) {
        debugmsg( "tried to insert a second handler for type %s into the DynamicDataLoader", type.c_str() );
    }
}

void DynamicDataLoader::add( const std::string &type,
                             const std::function<void( const JsonObject & )> &f )
{
    const auto pair = type_function_map.emplace( type, [f]( const JsonObject & obj, const std::string &,
    const std::string &, const std::string & ) {
        f( obj );
    } );
    if( !pair.second ) {
        debugmsg( "tried to insert a second handler for type %s into the DynamicDataLoader", type.c_str() );
    }
}

void DynamicDataLoader::initialize()
{
    // all of the applicable types that can be loaded, along with their loading functions
    // Add to this as needed with new StaticFunctionAccessors or new ClassFunctionAccessors for new applicable types
    // Static Function Access
    add( "WORLD_OPTION", &load_world_option );
    add( "EXTERNAL_OPTION", &load_external_option );
    add( "json_flag", &json_flag::load_all );
    add( "mutation_flag", &json_trait_flag::load_all );
    add( "fault", &fault::load_fault );
    add( "field_type", &field_types::load );
    add( "weather_type", &weather_types::load );
    add( "ammo_effect", &ammo_effects::load );
    add( "emit", &emit::load_emit );
    add( "activity_type", &activity_type::load );
    add( "vitamin", &vitamin::load_vitamin );
    add( "material", &materials::load );
    add( "bionic", &bionic_data::load_bionic );
    add( "profession", &profession::load_profession );
    add( "profession_item_substitutions", &profession::load_item_substitutions );
    add( "skill", &Skill::load_skill );
    add( "skill_display_type", &SkillDisplayType::load );
    add( "dream", &dreams::load );
    add( "mutation_category", &mutation_category_trait::load );
    add( "mutation_type", &load_mutation_type );
    add( "mutation", &mutation_branch::load_trait );
    add( "furniture", &load_furniture );
    add( "terrain", &load_terrain );
    add( "hunting_data", &hunting::snaring_hunting_data::load_hunting_data );
    add( "monstergroup", &MonsterGroupManager::LoadMonsterGroup );
    add( "MONSTER_BLACKLIST", &MonsterGroupManager::LoadMonsterBlacklist );
    add( "MONSTER_WHITELIST", &MonsterGroupManager::LoadMonsterWhitelist );
    add( "speech", &load_speech );
    add( "ammunition_type", &ammunition_type::load_ammunition_type );
    add( "start_location", &start_locations::load );
    add( "scenario", &scenario::load_scenario );
    add( "SCENARIO_BLACKLIST", &scen_blacklist::load_scen_blacklist );
    add( "skill_boost", &skill_boost::load_boost );
    add( "enchantment", &enchantment::load_enchantment );
    add( "hit_range", &Creature::load_hit_range );
    add( "scent_type", &scent_type::load_scent_type );
    add( "disease_type", &disease_type::load_disease_type );
    add( "ascii_art", &ascii_art::load_ascii_art );

    // json/colors.json would be listed here, but it's loaded before the others (see init_colors())
    // Non Static Function Access
    add( "snippet", []( const JsonObject & jo ) {
        SNIPPET.load_snippet( jo );
    } );
    add( "item_group", []( const JsonObject & jo ) {
        item_controller->load_item_group( jo );
    } );
    add( "trait_group", []( const JsonObject & jo ) {
        mutation_branch::load_trait_group( jo );
    } );
    add( "item_action", []( const JsonObject & jo ) {
        item_action_generator::generator().load_item_action( jo );
    } );

    add( "vehicle_part",  &vpart_info::load );
    add( "vehicle",  &vehicle_prototype::load );
    add( "vehicle_group",  &VehicleGroup::load );
    add( "vehicle_placement",  &VehiclePlacement::load );
    add( "vehicle_spawn",  &VehicleSpawn::load );

    add( "requirement", []( const JsonObject & jo ) {
        requirement_data::load_requirement( jo );
    } );
    add( "trap", &trap::load_trap );

    add( "AMMO", []( const JsonObject & jo, const std::string & src ) {
        item_controller->load_ammo( jo, src );
    } );
    add( "GUN", []( const JsonObject & jo, const std::string & src ) {
        item_controller->load_gun( jo, src );
    } );
    add( "ARMOR", []( const JsonObject & jo, const std::string & src ) {
        item_controller->load_armor( jo, src );
    } );
    add( "PET_ARMOR", []( const JsonObject & jo, const std::string & src ) {
        item_controller->load_pet_armor( jo, src );
    } );
    add( "TOOL", []( const JsonObject & jo, const std::string & src ) {
        item_controller->load_tool( jo, src );
    } );
    add( "TOOLMOD", []( const JsonObject & jo, const std::string & src ) {
        item_controller->load_toolmod( jo, src );
    } );
    add( "TOOL_ARMOR", []( const JsonObject & jo, const std::string & src ) {
        item_controller->load_tool_armor( jo, src );
    } );
    add( "BOOK", []( const JsonObject & jo, const std::string & src ) {
        item_controller->load_book( jo, src );
    } );
    add( "COMESTIBLE", []( const JsonObject & jo, const std::string & src ) {
        item_controller->load_comestible( jo, src );
    } );
    add( "CONTAINER", []( const JsonObject & jo, const std::string & src ) {
        item_controller->load_container( jo, src );
    } );
    add( "ENGINE", []( const JsonObject & jo, const std::string & src ) {
        item_controller->load_engine( jo, src );
    } );
    add( "WHEEL", []( const JsonObject & jo, const std::string & src ) {
        item_controller->load_wheel( jo, src );
    } );
    add( "FUEL", []( const JsonObject & jo, const std::string & src ) {
        item_controller->load_fuel( jo, src );
    } );
    add( "GUNMOD", []( const JsonObject & jo, const std::string & src ) {
        item_controller->load_gunmod( jo, src );
    } );
    add( "MAGAZINE", []( const JsonObject & jo, const std::string & src ) {
        item_controller->load_magazine( jo, src );
    } );
    add( "BATTERY", []( const JsonObject & jo, const std::string & src ) {
        item_controller->load_battery( jo, src );
    } );
    add( "GENERIC", []( const JsonObject & jo, const std::string & src ) {
        item_controller->load_generic( jo, src );
    } );
    add( "BIONIC_ITEM", []( const JsonObject & jo, const std::string & src ) {
        item_controller->load_bionic( jo, src );
    } );

    add( "ITEM_CATEGORY", &item_category::load_item_cat );

    add( "MIGRATION", []( const JsonObject & jo ) {
        item_controller->load_migration( jo );
    } );

    add( "charge_removal_blacklist", charge_removal_blacklist::load );
    add( "to_cbc_migration", to_cbc_migration::load );

    add( "MONSTER", []( const JsonObject & jo, const std::string & src ) {
        MonsterGenerator::generator().load_monster( jo, src );
    } );
    add( "SPECIES", []( const JsonObject & jo, const std::string & src ) {
        MonsterGenerator::generator().load_species( jo, src );
    } );

    add( "LOOT_ZONE", &zone_type::load_zones );
    add( "monster_adjustment", &load_monster_adjustment );
    add( "recipe_category", &load_recipe_category );
    add( "recipe",  &recipe_dictionary::load_recipe );
    add( "uncraft", &recipe_dictionary::load_uncraft );
    add( "recipe_group",  &recipe_group::load );

    add( "tool_quality", &quality::load_static );
    add( "technique", &load_technique );
    add( "weapon_category", &weapon_category::load_weapon_categories );
    add( "martial_art", &load_martial_art );
    add( "effect_type", &load_effect_type );
    add( "oter_id_migration", &overmap::load_oter_id_migration );
    add( "overmap_terrain", &overmap_terrains::load );
    add( "construction_category", &construction_categories::load );
    add( "construction_group", &construction_groups::load );
    add( "construction", &constructions::load );
    add( "mapgen", &load_mapgen );
    add( "overmap_land_use_code", &overmap_land_use_codes::load );
    add( "overmap_connection", &overmap_connections::load );
    add( "overmap_location", &overmap_locations::load );
    add( "overmap_special", &overmap_specials::load );
    add( "city_building", &city_buildings::load );
    add( "map_extra", &MapExtras::load );

    add( "region_settings", &load_region_settings );
    add( "region_overlay", &load_region_overlay );
    add( "ITEM_BLACKLIST", []( const JsonObject & jo ) {
        item_controller->load_item_blacklist( jo );
    } );
    add( "TRAIT_BLACKLIST", []( const JsonObject & jo ) {
        mutation_branch::load_trait_blacklist( jo );
    } );

    // loaded earlier.
    add( "colordef", &load_ignored_type );
    // mod information, ignored, handled by the mod manager
    add( "MOD_INFO", &load_ignored_type );

    add( "faction", &faction_template::load );
    add( "npc", &npc_template::load );
    add( "npc_class", &npc_class::load_npc_class );
    add( "talk_topic", &load_talk_topic );
    add( "behavior", &behavior::load_behavior );

    add( "MONSTER_FACTION", &monfactions::load_monster_faction );

    add( "sound_effect", &sfx::load_sound_effects );
    add( "sound_effect_preload", &sfx::load_sound_effect_preload );
    add( "playlist", &sfx::load_playlist );

    add( "gate", &gates::load );
    add( "overlay_order", &load_overlay_ordering );
    add( "mission_definition", []( const JsonObject & jo, const std::string & src ) {
        mission_type::load_mission_type( jo, src );
    } );
    add( "harvest", []( const JsonObject & jo, const std::string & src ) {
        harvest_list::load( jo, src );
    } );

    add( "monster_attack", []( const JsonObject & jo, const std::string & src ) {
        MonsterGenerator::generator().load_monster_attack( jo, src );
    } );
    add( "palette", mapgen_palette::load );
    add( "rotatable_symbol", &rotatable_symbols::load );
    add( "body_part", &body_part_type::load_bp );
    add( "anatomy", &anatomy::load_anatomy );
    add( "morale_type", &morale_type_data::load_type );
    add( "SPELL", &spell_type::load_spell );
    add( "clothing_mod", &clothing_mods::load );
    add( "ter_furn_transform", &ter_furn_transform::load_transform );
    add( "event_transformation", &event_transformation::load_transformation );
    add( "event_statistic", &event_statistic::load_statistic );
    add( "score", &score::load_score );
    add( "achievement", &achievement::load_achievement );
#if defined(TILES)
    add( "mod_tileset", &load_mod_tileset );
#else
    // No TILES - no tilesets
    add( "mod_tileset", &load_ignored_type );
#endif
}

void DynamicDataLoader::load_data_from_path( const std::string &path, const std::string &src,
        loading_ui &ui )
{
    assert( !finalized && "Can't load additional data after finalization.  Must be unloaded first." );
    // We assume that each folder is consistent in itself,
    // and all the previously loaded folders.
    // E.g. the core might provide a vpart "frame-x"
    // the first loaded mode might provide a vehicle that uses that frame
    // But not the other way round.

    // get a list of all files in the directory
    str_vec files = get_files_from_path( ".json", path, true, true );
    if( files.empty() ) {
        std::ifstream tmp( path.c_str(), std::ios::in );
        if( tmp ) {
            // path is actually a file, don't checking the extension,
            // assume we want to load this file anyway
            files.push_back( path );
        }
    }
    // iterate over each file
    for( auto &files_i : files ) {
        const std::string &file = files_i;
        // open the file as a stream
        cata_ifstream infile = std::move( cata_ifstream().mode( cata_ios_mode::binary ).open( file ) );
        // and stuff it into ram
        std::istringstream iss(
            std::string(
                ( std::istreambuf_iterator<char>( *infile ) ),
                std::istreambuf_iterator<char>()
            )
        );
        try {
            // parse it
            JsonIn jsin( iss, file );
            load_all_from_json( jsin, src, ui, path, file );
        } catch( const JsonError &err ) {
            throw std::runtime_error( err.what() );
        }
    }
}

void DynamicDataLoader::load_all_from_json( JsonIn &jsin, const std::string &src, loading_ui &,
        const std::string &base_path, const std::string &full_path )
{
    // TEMPORARY until 0.G: Remove single object support for consistency
    if( jsin.test_object() ) {
        // find type and dispatch single object
        JsonObject jo = jsin.get_object();
        load_object( jo, src, base_path, full_path );
        jo.finish();
        // if there's anything else in the file, it's an error.
        jsin.eat_whitespace();
        if( jsin.good() ) {
            jsin.error( string_format( "expected single-object file but found '%c'", jsin.peek() ) );
        }
    } else if( jsin.test_array() ) {
        jsin.start_array();
        // find type and dispatch each object until array close
        while( !jsin.end_array() ) {
            JsonObject jo = jsin.get_object();
            load_object( jo, src, base_path, full_path );
            jo.finish();
        }
    } else {
        // not an object or an array?
        jsin.error( "expected object or array" );
    }
    inp_mngr.pump_events();
}

void DynamicDataLoader::unload_data()
{
    finalized = false;

    //Moved to the top as a temp hack until vehicles are made into game objects
    vehicle_prototype::reset();
    cleanup_arenas();

    achievement::reset();
    activity_type::reset();
    ammo_effects::reset();
    ammunition_type::reset();
    anatomy::reset();
    ascii_art::reset();
    behavior::reset();
    bionic_data::reset();
    body_part_type::reset();
    charge_removal_blacklist::reset();
    clear_techniques_and_martial_arts();
    clothing_mods::reset();
    construction_categories::reset();
    construction_groups::reset();
    constructions::reset();
    Creature::reset_hit_range();
    disease_type::reset();
    dreams::clear();
    emit::reset();
    enchantment::reset();
    event_statistic::reset();
    event_transformation::reset();
    faction_template::reset();
    fault::reset();
    field_types::reset();
    gates::reset();
    harvest_list::reset();
    hunting::snaring_hunting_data::reset();
    item_action_generator::generator().reset();
    item_controller->reset();
    json_flag::reset();
    json_trait_flag::reset();
    MapExtras::reset();
    mapgen_palette::reset();
    materials::reset();
    mission_type::reset();
    monfactions::reset();
    MonsterGenerator::generator().reset();
    MonsterGroupManager::ClearMonsterGroups();
    morale_type_data::reset();
    mutation_branch::reset_all();
    mutation_category_trait::reset();
    mutations_category.clear();
    npc_class::reset_npc_classes();
    npc_template::reset();
    overmap_connections::reset();
    overmap_land_use_codes::reset();
    overmap_locations::reset();
    overmap_specials::reset();
    overmap_terrains::reset();
    overmap::reset_oter_id_migrations();
    profession::reset();
    quality::reset();
    recipe_dictionary::reset();
    recipe_group::reset();
    requirement_data::reset();
    reset_effect_types();
    reset_furn_ter();
    reset_mapgens();
    reset_monster_adjustment();
    reset_mutation_types();
    reset_overlay_ordering();
    reset_recipe_categories();
    reset_region_settings();
    reset_scenarios_blacklist();
    reset_speech();
    rotatable_symbols::reset();
    scenario::reset();
    scent_type::reset();
    score::reset();
    skill_boost::reset();
    Skill::reset();
    SkillDisplayType::reset();
    SNIPPET.clear_snippets();
    spell_type::reset_all();
    start_locations::reset();
    ter_furn_transform::reset_all();
    to_cbc_migration::reset();
    trap::reset();
    unload_talk_topics();
    VehicleGroup::reset();
    VehiclePlacement::reset();
    VehicleSpawn::reset();
    vitamin::reset();
    vpart_info::reset();
    weapon_category::reset();
    weather_types::reset();
    zone_type::reset_zones();
    l10n_data::unload_mod_catalogues();
#if defined(TILES)
    reset_mod_tileset();
#endif

    // Has to be cleaned last in case one of the above data collections
    // holds references to Lua functions or tables.
    lua.reset();
}

void DynamicDataLoader::finalize_loaded_data( loading_ui &ui )
{
    assert( !finalized && "Can't finalize the data twice." );
    assert( !stream_cache && "Expected stream cache to be null before finalization" );

    on_out_of_scope reset_stream_cache( [this]() {
        stream_cache.reset();
    } );
    stream_cache = std::make_unique<cached_streams>();

    ui.new_context( _( "Finalizing" ) );

    using named_entry = std::pair<std::string, std::function<void()>>;
    const std::vector<named_entry> entries = {{
            { _( "Flags" ), &json_flag::finalize_all },
            { _( "Mutation Flags" ), &json_trait_flag::finalize_all },
            { _( "Body parts" ), &body_part_type::finalize_all },
            { _( "Bionics" ), &bionic_data::finalize_all },
            { _( "Weather types" ), &weather_types::finalize_all },
            { _( "Field types" ), &field_types::finalize_all },
            { _( "Ammo effects" ), &ammo_effects::finalize_all },
            { _( "Emissions" ), &emit::finalize },
            {
                _( "Items" ), []()
                {
                    item_controller->finalize();
                }
            },
            {
                _( "Crafting requirements" ), []()
                {
                    requirement_data::finalize();
                }
            },
            { _( "Vehicle parts" ), &vpart_info::finalize },
            { _( "Traps" ), &trap::finalize },
            { _( "Terrain" ), &set_ter_ids },
            { _( "Furniture" ), &finalize_furn },
            { _( "Overmap land use codes" ), &overmap_land_use_codes::finalize },
            { _( "Overmap terrain" ), &overmap_terrains::finalize },
            { _( "Overmap connections" ), &overmap_connections::finalize },
            { _( "Overmap specials" ), &overmap_specials::finalize },
            { _( "Overmap locations" ), &overmap_locations::finalize },
            { _( "Start locations" ), &start_locations::finalize_all },
            { _( "Zone manager" ), &zone_manager::reset_manager },
            { _( "Vehicle prototypes" ), &vehicle_prototype::finalize },
            { _( "Mapgen weights" ), &calculate_mapgen_weights },
            { _( "Mapgen parameters" ), &overmap_specials::finalize_mapgen_parameters },
            {
                _( "Monster types" ), []()
                {
                    MonsterGenerator::generator().finalize_mtypes();
                }
            },
            { _( "Monster groups" ), &MonsterGroupManager::FinalizeMonsterGroups },
            { _( "Monster factions" ), &monfactions::finalize },
            { _( "Factions" ), &npc_factions::finalize },
            { _( "Constructions" ), &constructions::finalize },
            { _( "Crafting recipes" ), &recipe_dictionary::finalize },
            { _( "Recipe groups" ), &recipe_group::check },
            { _( "Martial arts" ), &finialize_martial_arts },
            { _( "NPC classes" ), &npc_class::finalize_all },
            { _( "Missions" ), &mission_type::finalize },
            { _( "Behaviors" ), &behavior::finalize },
            { _( "Harvest lists" ), &harvest_list::finalize_all },
            { _( "Anatomies" ), &anatomy::finalize_all },
            { _( "Mutations" ), &mutation_branch::finalize },
            { _( "Achievements" ), &achievement::finalize },
            { _( "Localization" ), &l10n_data::load_mod_catalogues },
#if defined(TILES)
            { _( "Tileset" ), &load_tileset },
#endif
        }
    };

    for( const named_entry &e : entries ) {
        ui.add_entry( e.first );
    }

    ui.show();
    for( const named_entry &e : entries ) {
        e.second();
        ui.proceed();
    }
}

void DynamicDataLoader::check_consistency( loading_ui &ui )
{
    ui.new_context( _( "Verifying" ) );

    using named_entry = std::pair<std::string, std::function<void()>>;
    const std::vector<named_entry> entries = {{
            { _( "Flags" ), &json_flag::check_consistency },
            { _( "Mutation Flags" ), &json_trait_flag::check_consistency },
            {
                _( "Crafting requirements" ), []()
                {
                    requirement_data::check_consistency();
                }
            },
            { _( "Vitamins" ), &vitamin::check_consistency },
            { _( "Weather types" ), &weather_types::check_consistency },
            { _( "Field types" ), &field_types::check_consistency },
            { _( "Hunting data" ), &hunting::snaring_hunting_data::check_consistency },
            { _( "Ammo effects" ), &ammo_effects::check_consistency },
            { _( "Emissions" ), &emit::check_consistency },
            { _( "Activities" ), &activity_type::check_consistency },
            {
                _( "Items" ), []()
                {
                    item_controller->check_definitions();
                }
            },
            { _( "Materials" ), &materials::check },
            { _( "Engine faults" ), &fault::check_consistency },
            { _( "Vehicle parts" ), &vpart_info::check },
            { _( "Mapgen definitions" ), &check_mapgen_definitions },
            { _( "Mapgen palettes" ), &mapgen_palette::check_definitions },
            {
                _( "Monster types" ), []()
                {
                    MonsterGenerator::generator().check_monster_definitions();
                }
            },
            { _( "Monster groups" ), &MonsterGroupManager::check_group_definitions },
            { _( "Furniture and terrain" ), &check_furniture_and_terrain },
            { _( "Constructions" ), &constructions::check_consistency },
            { _( "Construction sequences" ), &constructions::check_consistency },
            { _( "Professions" ), &profession::check_definitions },
            { _( "Scenarios" ), &scenario::check_definitions },
            { _( "Martial arts" ), &check_martialarts },
            { _( "Mutations" ), &mutation_branch::check_consistency },
            { _( "Mutation Categories" ), &mutation_category_trait::check_consistency },
            { _( "Overmap land use codes" ), &overmap_land_use_codes::check_consistency },
            { _( "Overmap connections" ), &overmap_connections::check_consistency },
            { _( "Overmap terrain" ), &overmap_terrains::check_consistency },
            { _( "Overmap locations" ), &overmap_locations::check_consistency },
            { _( "Overmap specials" ), &overmap_specials::check_consistency },
            { _( "Map extras" ), &MapExtras::check_consistency },
            { _( "Start locations" ), &start_locations::check_consistency },
            { _( "Regional settings" ), &check_regional_settings },
            { _( "Ammunition types" ), &ammunition_type::check_consistency },
            { _( "Traps" ), &trap::check_consistency },
            { _( "Bionics" ), &bionic_data::check_consistency },
            { _( "Gates" ), &gates::check },
            { _( "NPC classes" ), &npc_class::check_consistency },
            { _( "Behaviors" ), &behavior::check_consistency },
            { _( "Mission types" ), &mission_type::check_consistency },
            {
                _( "Item actions" ), []()
                {
                    item_action_generator::generator().check_consistency();
                }
            },
            { _( "Harvest lists" ), &harvest_list::check_consistency },
            { _( "NPC templates" ), &npc_template::check_consistency },
            { _( "Body parts" ), &body_part_type::check_consistency },
            { _( "Anatomies" ), &anatomy::check_consistency },
            { _( "Spells" ), &spell_type::check_consistency },
            { _( "Enchantments" ), &enchantment::check_consistency },
            { _( "Transformations" ), &event_transformation::check_consistency },
            { _( "Statistics" ), &event_statistic::check_consistency },
            { _( "Scent types" ), &scent_type::check_scent_consistency },
            { _( "Scores" ), &score::check_consistency },
            { _( "Achievements" ), &achievement::check_consistency },
            { _( "Disease types" ), &disease_type::check_disease_consistency },
            { _( "Factions" ), &faction_template::check_consistency },
            { _( "Effects" ), &effect_type::check_consistency },
        }
    };

    for( const named_entry &e : entries ) {
        ui.add_entry( e.first );
    }

    ui.show();
    for( const named_entry &e : entries ) {
        e.second();
        ui.proceed();
    }

    finalized = true;
}

/**
 * Load & finalize specified content packs.
 * @param ui structure for load progress display
 * @param msg string to display whilst loading prompt
 * @param packs content packs to load in correct dependent order
 */
static void load_and_finalize_packs( loading_ui &ui, const std::string &msg,
                                     const std::vector<mod_id> &packs )
{
    ui.new_context( msg );
    std::vector<mod_id> missing;
    std::vector<mod_id> available;

    for( const mod_id &e : packs ) {
        if( e.is_valid() ) {
            available.emplace_back( e );
            ui.add_entry( e->name() );
        } else {
            missing.push_back( e );
        }
    }

    for( const mod_id &e : missing ) {
        debugmsg( "unknown content pack [%s]", e );
    }

    DynamicDataLoader &loader = DynamicDataLoader::get_instance();

    loader.lua = cata::make_wrapped_state();

    cata::init_global_state_tables( *loader.lua, available );

    ui.show();
    for( const mod_id &mod : available ) {
        if( mod->lua_api_version ) {
            if( cata::get_lua_api_version() != *mod->lua_api_version ) {
                // The mod may be broken, but let's be user-friendly and try to load it anyway
                debugmsg(
                    "Content pack uses outdated Lua API (current: %d, uses: %d) %s [%s]",
                    cata::get_lua_api_version(), *mod->lua_api_version,
                    mod->name(), mod
                );
            }
            cata::set_mod_being_loaded( *loader.lua, mod );
            cata::run_mod_preload_script( *loader.lua, mod );
        }
    }

    cata::reg_lua_iuse_actors( *loader.lua, *item_controller );

    for( const mod_id &mod : available ) {
        loader.load_data_from_path( mod->path, mod.str(), ui );
        ui.proceed();
    }

    loader.finalize_loaded_data( ui );

    for( const mod_id &mod : available ) {
        if( mod->lua_api_version ) {
            cata::set_mod_being_loaded( *loader.lua, mod );
            cata::run_mod_finalize_script( *loader.lua, mod );
        }
    }

    loader.check_consistency( ui );

    init::load_main_lua_scripts( *loader.lua, packs );
    cata::clear_mod_being_loaded( *loader.lua );
}

auto init::load_main_lua_scripts( cata::lua_state &state, const std::vector<mod_id> &packs ) -> int
{
    state.lua.script( R"(
        for k, v in pairs(package.loaded)
            do package.loaded[k] = nil
        end
    )" );
    auto range = packs | std::views::filter( []( const mod_id & mod ) { return mod.is_valid() && mod->lua_api_version; } );
    for( const auto &mod : range ) {
        cata::set_mod_being_loaded( state, mod );
        cata::run_mod_main_script( state, mod );
    }
    return std::ranges::distance( range );
}

bool init::is_data_loaded()
{
    return DynamicDataLoader::get_instance().is_data_finalized();
}

static void clear_loaded_data()
{
    DynamicDataLoader::get_instance().unload_data();
}

void init::load_core_bn_modfiles()
{
    clear_loaded_data();

    loading_ui ui( false );
    load_and_finalize_packs(
        ui, _( "Loading content packs" ),
    { mod_management::get_default_core_content_pack() }
    );
}

void init::load_world_modfiles( loading_ui &ui, const world *world,
                                const std::string &artifacts_file )
{
    clear_loaded_data();

    mod_management::t_mod_list &mods = world->info->active_mod_order;

    // remove any duplicates whilst preserving order (fixes #19385)
    std::set<mod_id> found;
    mods.erase( std::remove_if( mods.begin(), mods.end(), [&found]( const mod_id & e ) {
        if( found.contains( e ) ) {
            return true;
        } else {
            found.insert( e );
            return false;
        }
    } ), mods.end() );

    // require at least one core mod (saves before version 6 may implicitly require dda pack)
    if( std::none_of( mods.begin(), mods.end(), []( const mod_id & e ) {
    return e->core;
} ) ) {
        mods.insert( mods.begin(), mod_management::get_default_core_content_pack() );
    }

    // TODO: get rid of artifacts
    load_artifacts( world, artifacts_file );

    // this code does not care about mod dependencies,
    // it assumes that those dependencies are static and
    // are resolved during the creation of the world.
    // That means world->active_mod_order contains a list
    // of mods in the correct order.
    load_and_finalize_packs( ui, _( "Loading files" ), mods );
}

bool init::check_mods_for_errors( loading_ui &ui, const std::vector<mod_id> &opts )
{
    const dependency_tree &tree = world_generator->get_mod_manager().get_tree();

    // Deduplicated list of mods to check
    std::set<mod_id> to_check;

    for( const mod_id &id : opts ) {
        if( !id.is_valid() ) {
            std::cerr << string_format( "Unknown mod: [%s]\n", id );
            return false;
        }

        if( !tree.is_available( id ) ) {
            std::cerr << string_format(
                          "Missing dependencies: %s %s\n",
                          id, tree.get_node( id )->s_errors()
                      );
            return false;
        }

        to_check.emplace( id );
    }

    // If no specific mods specified check all non-obsolete mods
    if( to_check.empty() ) {
        for( const mod_id &mod : world_generator->get_mod_manager().all_mods() ) {
            if( !mod->obsolete ) {
                to_check.emplace( mod );
            }
        }
    }
    // If no mods are available then test core data only
    if( to_check.empty() ) {
        to_check.emplace( mod_management::get_default_core_content_pack() );
    }

    for( const mod_id &id : to_check ) {
        clear_loaded_data();

        world_generator->set_active_world( nullptr );
        world_generator->init();
        const std::vector<mod_id> mods_empty;
        WORLDINFO *test_world = world_generator->make_new_world( mods_empty );
        if( !test_world ) {
            std::cerr << "Failed to generate test world." << '\n';
            return false;
        }
        world_generator->set_active_world( test_world );

        std::cout << string_format( "Checking mod %s [%s]\n", id->name(), id );

        // Load all dependencies first
        std::vector<mod_id> mods_list = tree.get_dependencies_of_X_as_strings( id );
        // Load the mod itself
        mods_list.push_back( id );

        try {
            load_and_finalize_packs( ui, _( "Checking mods" ), mods_list );
        } catch( const std::exception &err ) {
            std::cerr << "Error loading data: " << err.what() << '\n';
        }

        std::string world_name = world_generator->active_world->info->world_name;
        world_generator->delete_world( world_name, true );

        // TODO: Why would we need these calls?
        MAPBUFFER.clear();
        overmap_buffer.clear();
    }

    return !debug_has_error_been_observed();
}

void init::load_soundpack_files( const std::string &soundpack_path )
{
    // Leverage DynamicDataLoader to load a soundpack.
    // It's not a mod, so we avoid the regular mod loading routines.
    // clear_loaded_data() is not needed here, tileset gets loaded on game init before any mods
    loading_ui ui( false );
    DynamicDataLoader::get_instance().load_data_from_path( soundpack_path, "sound_core", ui );
}
