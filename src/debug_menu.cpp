#include "debug_menu.h"

// IWYU pragma: no_include <cxxabi.h>

#include <algorithm>
#include <numeric>
#include <array>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <limits>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "action.h"
#include "artifact.h"
#include "avatar.h"
#include "bodypart.h"
#include "calendar.h"
#include "cata_utility.h"
#include "catacharset.h"
#include "catalua.h"
#include "character.h"
#include "character_display.h"
#include "character_id.h"
#include "character_martial_arts.h"
#include "color.h"
#include "coordinate_conversions.h"
#include "coordinates.h"
#include "cursesdef.h"
#include "debug.h"
#include "effect.h"
#include "enum_conversions.h"
#include "enums.h"
#include "faction.h"
#include "filesystem.h"
#include "game.h"
#include "game_constants.h"
#include "game_inventory.h"
#include "input.h"
#include "inventory.h"
#include "item.h"
#include "item_group.h"
#include "json.h"
#include "json_export.h"
#include "language.h"
#include "magic.h"
#include "map.h"
#include "map_extras.h"
#include "map_iterator.h"
#include "mapgen.h"
#include "mapgendata.h"
#include "martialarts.h"
#include "memory_fast.h"
#include "messages.h"
#include "mission.h"
#include "monster.h"
#include "monstergenerator.h"
#include "morale_types.h"
#include "mtype.h"
#include "npc.h"
#include "npc_class.h"
#include "omdata.h"
#include "options.h"
#include "output.h"
#include "overmap.h"
#include "overmap_ui.h"
#include "overmapbuffer.h"
#include "pimpl.h"
#include "player.h"
#include "pldata.h"
#include "point.h"
#include "popup.h"
#include "recipe_dictionary.h"
#include "rng.h"
#include "sounds.h"
#include "stomach.h"
#include "string_formatter.h"
#include "string_id.h"
#include "string_input_popup.h"
#include "string_utils.h"
#include "trait_group.h"
#include "translations.h"
#include "type_id.h"
#include "ui.h"
#include "ui_manager.h"
#include "uistate.h"
#include "units.h"
#include "units_utility.h"
#include "url_utility.h"
#include "vehicle.h"
#include "vehicle_part.h"
#include "veh_type.h"
#include "vitamin.h"
#include "vpart_position.h"
#include "weather.h"
#include "weather_gen.h"
#include "weighted_list.h"
#include "game_info.h"
#include "overmap_special.h"

static const mtype_id mon_generator( "mon_generator" );

extern std::map<std::string, weighted_int_list<std::shared_ptr<mapgen_function_json_nested>> >
        nested_mapgen;

#if defined(TILES)
#include "sdl_wrappers.h"
#include "cata_tiles.h"
#include "dynamic_atlas.h"
#include "sdltiles.h"
#endif

namespace debug_menu
{

enum debug_menu_index {
    DEBUG_WISH,
    DEBUG_SHORT_TELEPORT,
    DEBUG_LONG_TELEPORT,
    DEBUG_TELEPORT_TO_OVERMAP_SPECIAL,
    DEBUG_REVEAL_MAP,
    DEBUG_SPAWN_NPC,
    DEBUG_SPAWN_MON,
    DEBUG_GAME_STATE,
    DEBUG_REPRODUCE_AREA,
    DEBUG_ERASE_ITEMS_AREA,
    DEBUG_KILL_AREA,
    DEBUG_KILL_NPCS,
    DEBUG_MUTATE,
    DEBUG_SPAWN_VEHICLE,
    DEBUG_EDIT_PLAYER,
    DEBUG_SPAWN_ARTIFACT,
    DEBUG_SPAWN_CLAIRVOYANCE,
    DEBUG_MAP_EDITOR,
    DEBUG_CHANGE_WEATHER,
    DEBUG_WIND_DIRECTION,
    DEBUG_WIND_SPEED,
    DEBUG_GEN_SOUND,
    DEBUG_KILL_MONS,
    DEBUG_DISPLAY_HORDES,
    DEBUG_TEST_IT_GROUP,
    DEBUG_DAMAGE_SELF,
    DEBUG_SHOW_SOUND,
    DEBUG_DISPLAY_WEATHER,
    DEBUG_DISPLAY_SCENTS,
    DEBUG_DISPLAY_DISTRIBUTION_GRIDS,
    DEBUG_CHANGE_TIME,
    DEBUG_SET_AUTOMOVE,
    DEBUG_SHOW_MUT_CAT,
    DEBUG_SHOW_MUT_CHANCES,
    DEBUG_OM_EDITOR,
    DEBUG_BENCHMARK,
    DEBUG_BENCHMARK_FPS,
    DEBUG_OM_TELEPORT,
    DEBUG_OM_TELEPORT_COORDINATES,
    DEBUG_TRAIT_GROUP,
    DEBUG_SHOW_MSG,
    DEBUG_CRASH_GAME,
    DEBUG_RELOAD_TRANSLATIONS,
    DEBUG_MAP_EXTRA,
    DEBUG_DISPLAY_NPC_PATH,
    DEBUG_PRINT_FACTION_INFO,
    DEBUG_PRINT_NPC_MAGIC,
    DEBUG_QUIT_NOSAVE,
    DEBUG_LUA_CONSOLE,
    DEBUG_TEST_WEATHER,
    DEBUG_SAVE_SCREENSHOT,
    DEBUG_BUG_REPORT,
    DEBUG_GAME_REPORT,
    DEBUG_JOIN_DISCORD,
    DEBUG_DISPLAY_SCENTS_LOCAL,
    DEBUG_DISPLAY_SCENTS_TYPE_LOCAL,
    DEBUG_DISPLAY_TEMP,
    DEBUG_DISPLAY_VEHICLE_AI,
    DEBUG_DISPLAY_VISIBILITY,
    DEBUG_DISPLAY_LIGHTING,
    DEBUG_DISPLAY_RADIATION,
    DEBUG_DISPLAY_TRANSPARENCY,
    DEBUG_DISPLAY_SUBMAP_GRID,
    DEBUG_TEST_MAP_EXTRA_DISTRIBUTION,
    DEBUG_VEHICLE_BATTERY_CHARGE,
    DEBUG_VEHICLE_EXPORT_JSON,
    DEBUG_HOUR_TIMER,
    DEBUG_NESTED_MAPGEN,
    DEBUG_RESET_IGNORED_MESSAGES,
    DEBUG_RELOAD_TILES,
    DEBUG_SWAP_CHAR,
    DEBUG_DUMP_TILES,
    DEBUG_DISPLAY_TILESET_NO_VFX
};

class mission_debug
{
    private:
        // Doesn't actually "destroy" the mission, just removes assignments
        static void remove_mission( mission &m );
    public:
        static void edit_mission( mission &m );
        static void edit( Character &who );
        static void edit_player();
        static void control_npc_menu();
        static void edit_npc( npc &who );
        static std::string describe( const mission &m );
};

static int info_uilist( bool display_all_entries = true )
{
    // always displayed
    std::vector<uilist_entry> uilist_initializer = {
        { uilist_entry( DEBUG_SAVE_SCREENSHOT, true, 'H', _( "Take screenshot" ) ) },
        { uilist_entry( DEBUG_BUG_REPORT, true, 'U', _( "Submit a bug report on github" ) ) },
        { uilist_entry( DEBUG_GAME_REPORT, true, 'r', _( "Generate game report" ) ) },
        { uilist_entry( DEBUG_JOIN_DISCORD, true, 'J', _( "Join the Discord" ) ) },
    };

    if( display_all_entries ) {
        const std::vector<uilist_entry> debug_only_options = {
            { uilist_entry( DEBUG_GAME_STATE, true, 'g', _( "Check game state" ) ) },
            { uilist_entry( DEBUG_DISPLAY_HORDES, true, 'h', _( "Display hordes" ) ) },
            { uilist_entry( DEBUG_TEST_IT_GROUP, true, 'i', _( "Test item group" ) ) },
            { uilist_entry( DEBUG_SHOW_SOUND, true, 'c', _( "Show sound clustering" ) ) },
            { uilist_entry( DEBUG_DISPLAY_WEATHER, true, 'w', _( "Display weather" ) ) },
            { uilist_entry( DEBUG_DISPLAY_SCENTS, true, 'S', _( "Display overmap scents" ) ) },
            { uilist_entry( DEBUG_DISPLAY_DISTRIBUTION_GRIDS, true, 'G', _( "Display overmap distribution grids" ) ) },
            { uilist_entry( DEBUG_DISPLAY_SCENTS_LOCAL, true, 's', _( "Toggle display local scents" ) ) },
            { uilist_entry( DEBUG_DISPLAY_SCENTS_TYPE_LOCAL, true, 'y', _( "Toggle display local scents type" ) ) },
            { uilist_entry( DEBUG_DISPLAY_TEMP, true, 'T', _( "Toggle display temperature" ) ) },
            { uilist_entry( DEBUG_DISPLAY_VEHICLE_AI, true, 'V', _( "Toggle display vehicle autopilot overlay" ) ) },
            { uilist_entry( DEBUG_DISPLAY_VISIBILITY, true, 'v', _( "Toggle display visibility" ) ) },
            { uilist_entry( DEBUG_DISPLAY_LIGHTING, true, 'l', _( "Toggle display lighting" ) ) },
            { uilist_entry( DEBUG_DISPLAY_TRANSPARENCY, true, 'p', _( "Toggle display transparency" ) ) },
            { uilist_entry( DEBUG_DISPLAY_RADIATION, true, 'R', _( "Toggle display radiation" ) ) },
            { uilist_entry( DEBUG_DISPLAY_SUBMAP_GRID, true, 'o', _( "Toggle display submap grid" ) ) },
#if defined(TILES)
            { uilist_entry( ACTION_TOGGLE_ZONE_OVERLAY, true, 'z', _( "Toggle zone overlay" ) ) },
#endif
            { uilist_entry( DEBUG_SET_AUTOMOVE, true, 'A', _( "Set automove target" ) ) },
            { uilist_entry( DEBUG_SHOW_MUT_CAT, true, 'm', _( "Show mutation category levels" ) ) },
            { uilist_entry( DEBUG_SHOW_MUT_CHANCES, true, 'u', _( "Show mutation trait chances" ) ) },
            { uilist_entry( DEBUG_BENCHMARK, true, 'b', _( "Draw benchmark" ) ) },
            { uilist_entry( DEBUG_BENCHMARK_FPS, true, 'B', _( "FPS benchmark" ) ) },
            { uilist_entry( DEBUG_HOUR_TIMER, true, 'E', _( "Toggle hour timer" ) ) },
            { uilist_entry( DEBUG_TRAIT_GROUP, true, 't', _( "Test trait group" ) ) },
            { uilist_entry( DEBUG_SHOW_MSG, true, 'd', _( "Show debug message" ) ) },
            { uilist_entry( DEBUG_CRASH_GAME, true, 'C', _( "Crash game (test crash handling)" ) ) },
            { uilist_entry( DEBUG_RELOAD_TRANSLATIONS, true, 'L', _( "Reload translations" ) ) },
            { uilist_entry( DEBUG_DISPLAY_NPC_PATH, true, 'n', _( "Toggle NPC pathfinding on map" ) ) },
            { uilist_entry( DEBUG_PRINT_FACTION_INFO, true, 'f', _( "Print faction info to console" ) ) },
            { uilist_entry( DEBUG_PRINT_NPC_MAGIC, true, 'M', _( "Print NPC magic info to console" ) ) },
            { uilist_entry( DEBUG_TEST_WEATHER, true, 'W', _( "Test weather" ) ) },
            { uilist_entry( DEBUG_TEST_MAP_EXTRA_DISTRIBUTION, true, 'e', _( "Test map extra list" ) ) },
            { uilist_entry( DEBUG_RESET_IGNORED_MESSAGES, true, 'I', _( "Reset ignored debug messages" ) ) },
            { uilist_entry( DEBUG_SWAP_CHAR, true, 'x', _( "Control NPC follower" ) ) },
#if defined(TILES)
            { uilist_entry( DEBUG_RELOAD_TILES, true, 'D', _( "Reload tileset and show missing tiles" ) ) },
#endif
#if defined(TILES) && defined(DYNAMIC_ATLAS)
            { uilist_entry( DEBUG_DUMP_TILES, true, 'F', _( "Dump dynamic tile atlas" ) ) },
            { uilist_entry( DEBUG_DISPLAY_TILESET_NO_VFX, true, 'j', _( "Toggle tileset visual effects" ) ) },
#endif
        };
        uilist_initializer.insert( uilist_initializer.begin(), debug_only_options.begin(),
                                   debug_only_options.end() );
    }

    return uilist( _( "Info…" ), uilist_initializer );
}

static int vehicle_uilist()
{
    std::vector<uilist_entry> uilist_initializer = {
        { uilist_entry( DEBUG_VEHICLE_BATTERY_CHARGE, true, 'b', _( "Change [b]attery charge" ) ) },
        { uilist_entry( DEBUG_VEHICLE_EXPORT_JSON, true, 'j', _( "Export [j]son template" ) ) },
    };

    return uilist( _( "Vehicle…" ), uilist_initializer );
}

static int teleport_uilist()
{
    const std::vector<uilist_entry> uilist_initializer = {
        { uilist_entry( DEBUG_SHORT_TELEPORT, true, 's', _( "Teleport - short range" ) ) },
        { uilist_entry( DEBUG_LONG_TELEPORT, true, 'l', _( "Teleport - long range" ) ) },
        { uilist_entry( DEBUG_TELEPORT_TO_OVERMAP_SPECIAL, true, 'a', _( "Teleport - overmap special" ) ) },
        { uilist_entry( DEBUG_OM_TELEPORT, true, 'o', _( "Teleport - adjacent overmap" ) ) },
        { uilist_entry( DEBUG_OM_TELEPORT_COORDINATES, true, 'p', _( "Teleport - specific overmap coordinates" ) ) },
    };

    return uilist( _( "Teleport…" ), uilist_initializer );
}

static int spawning_uilist()
{
    const std::vector<uilist_entry> uilist_initializer = {
        { uilist_entry( DEBUG_WISH, true, 'w', _( "Spawn an item" ) ) },
        { uilist_entry( DEBUG_SPAWN_NPC, true, 'n', _( "Spawn NPC" ) ) },
        { uilist_entry( DEBUG_SPAWN_MON, true, 'm', _( "Spawn monster" ) ) },
        { uilist_entry( DEBUG_SPAWN_VEHICLE, true, 'v', _( "Spawn a vehicle" ) ) },
        { uilist_entry( DEBUG_SPAWN_ARTIFACT, true, 'a', _( "Spawn artifact" ) ) },
        { uilist_entry( DEBUG_SPAWN_CLAIRVOYANCE, true, 'c', _( "Spawn clairvoyance artifact" ) ) },
    };

    return uilist( _( "Spawning…" ), uilist_initializer );
}

static int map_uilist()
{
    const std::vector<uilist_entry> uilist_initializer = {
        { uilist_entry( DEBUG_REVEAL_MAP, true, 'r', _( "Reveal map" ) ) },
        { uilist_entry( DEBUG_REPRODUCE_AREA, true, 'R', _( "Reproduce in Area" ) ) },
        { uilist_entry( DEBUG_ERASE_ITEMS_AREA, true, 'e', _( "Erase items in Area" ) ) },
        { uilist_entry( DEBUG_KILL_AREA, true, 'a', _( "Kill in Area" ) ) },
        { uilist_entry( DEBUG_KILL_NPCS, true, 'k', _( "Kill NPCs" ) ) },
        { uilist_entry( DEBUG_MAP_EDITOR, true, 'M', _( "Map editor" ) ) },
        { uilist_entry( DEBUG_CHANGE_WEATHER, true, 'w', _( "Change weather" ) ) },
        { uilist_entry( DEBUG_WIND_DIRECTION, true, 'd', _( "Change wind direction" ) ) },
        { uilist_entry( DEBUG_WIND_SPEED, true, 's', _( "Change wind speed" ) ) },
        { uilist_entry( DEBUG_GEN_SOUND, true, 'S', _( "Generate sound" ) ) },
        { uilist_entry( DEBUG_KILL_MONS, true, 'K', _( "Kill all monsters" ) ) },
        { uilist_entry( DEBUG_CHANGE_TIME, true, 't', _( "Change time" ) ) },
        { uilist_entry( DEBUG_OM_EDITOR, true, 'O', _( "Overmap editor" ) ) },
        { uilist_entry( DEBUG_MAP_EXTRA, true, 'm', _( "Spawn map extra" ) ) },
        { uilist_entry( DEBUG_NESTED_MAPGEN, true, 'n', _( "Spawn nested mapgen" ) ) },
    };

    return uilist( _( "Map…" ), uilist_initializer );
}

/**
 * Create the debug menu UI list.
 * @param display_all_entries: `true` if all entries should be displayed, `false` is some entries should be hidden (for ex. when the debug menu is called from the main menu).
 *   This allows to have some menu elements at the same time in the main menu and in the debug menu.
 * @returns The chosen action.
 */
static int debug_menu_uilist( bool display_all_entries = true )
{
    std::vector<uilist_entry> menu = {
        { uilist_entry( 1, true, 'i', _( "Info…" ) ) },
    };

    if( display_all_entries ) {
        const std::vector<uilist_entry> debug_menu = {
            { uilist_entry( DEBUG_QUIT_NOSAVE, true, 'Q', _( "Quit to main menu" ) )  },
            { uilist_entry( 2, true, 's', _( "Spawning…" ) ) },
            { uilist_entry( 3, true, 'p', _( "Player…" ) ) },
            { uilist_entry( 6, true, 'v', _( "Vehicle…" ) ) },
            { uilist_entry( 4, true, 't', _( "Teleport…" ) ) },
            { uilist_entry( 5, true, 'm', _( "Map…" ) ) },
        };

        // insert debug-only menu right after "Info".
        menu.insert( menu.begin() + 1, debug_menu.begin(), debug_menu.end() );
        menu.emplace_back( 7, true, 'l', _( "Lua console" ) );
    }

    std::string msg;
    if( display_all_entries ) {
        msg = _( "Debug Functions - Manipulate the fabric of reality!\nYou can use them to fix a bug or test something.\nBe careful, as some of them may potentially break things." );
    } else {
        msg = _( "Debug Functions" );
    }

    while( true ) {
        const int group = uilist( msg, menu );

        int action;

        switch( group ) {
            case DEBUG_QUIT_NOSAVE:
                action = DEBUG_QUIT_NOSAVE;
                break;
            case 1:
                action = info_uilist( display_all_entries );
                break;
            case 2:
                action = spawning_uilist();
                break;
            case 3:
                action = -1;
                debug_menu::character_edit_menu( get_player_character() );
                break;
            case 4:
                action = teleport_uilist();
                break;
            case 5:
                action = map_uilist();
                break;
            case 6:
                action = vehicle_uilist();
                break;
            case 7:
                action = DEBUG_LUA_CONSOLE;
                break;
            default:
                return group;
        }
        if( action >= 0 ) {
            return action;
        }
    }
}

void teleport_short()
{
    const std::optional<tripoint> where = g->look_around( true );
    if( !where || *where == g->u.pos() ) {
        return;
    }
    g->place_player( *where );
    const tripoint new_pos( g->u.pos() );
    add_msg( _( "You teleport to point (%d,%d,%d)." ), new_pos.x, new_pos.y, new_pos.z );
}

void teleport_long()
{
    const tripoint_abs_omt where( ui::omap::choose_point() );
    if( where == overmap::invalid_tripoint ) {
        return;
    }
    g->place_player_overmap( where );
    add_msg( _( "You teleport to submap %s." ), where.to_string() );
}

void teleport_overmap( bool specific_coordinates )
{
    tripoint_abs_omt where;
    if( specific_coordinates ) {
        const std::string text = string_input_popup()
                                 .title( "Teleport where?" )
                                 .width( 20 )
                                 .query_string();
        if( text.empty() ) {
            return;
        }
        const std::vector<std::string> coord_strings = string_split( text, ',' );
        tripoint coord;
        coord.x = !coord_strings.empty() ? std::atoi( coord_strings[0].c_str() ) : 0;
        coord.y = coord_strings.size() >= 2 ? std::atoi( coord_strings[1].c_str() ) : 0;
        coord.z = coord_strings.size() >= 3 ? std::atoi( coord_strings[2].c_str() ) : 0;
        where = tripoint_abs_omt( OMAPX * coord.x, OMAPY * coord.y, coord.z );
    } else {
        const std::optional<tripoint> dir_ = choose_direction( _( "Where is the desired overmap?" ) );
        if( !dir_ ) {
            return;
        }
        const tripoint offset = tripoint( OMAPX * dir_->x, OMAPY * dir_->y, dir_->z );
        where = g->u.global_omt_location() + offset;
    }

    g->place_player_overmap( where );

    const tripoint_abs_om new_pos = project_to<coords::om>( g->u.global_omt_location() );
    add_msg( _( "You teleport to overmap %s." ), new_pos.to_string() );
}

static void teleport_to_overmap_special()
{
    std::vector<const overmap_special *> vec_os;
    std::vector<std::pair<std::string, vproto_id>> area_strings;

    uilist area_menu;
    for( const overmap_special &elem : overmap_specials::get_all() ) {
        vec_os.push_back( &elem );
        const std::string entry_text = elem.id.str();
        area_menu.addentry( -1, true, 0, entry_text );
    }

    area_menu.text = _( "Choose area to teleport to" );
    area_menu.query( true );
    if( area_menu.ret == UILIST_CANCEL ) {
        return;
    }

    const overmap_special *target = vec_os[area_menu.ret];
    const overmap_special_id target_id = target->id;
    mission_target_params t;
    t.overmap_terrain = target->get_terrain_at( tripoint() )->get_mapgen_id();
    t.overmap_special = target_id;
    t.search_range = 0;
    t.reveal_radius = 3;

    g->place_player_overmap( mission_util::get_om_terrain_pos( t ) );
    add_msg( _( "You teleport to submap %s." ), target_id );
}

void spawn_nested_mapgen()
{
    uilist nest_menu;
    std::vector<std::string> nest_str;
    for( auto &nested : nested_mapgen ) {
        nest_menu.addentry( -1, true, -1, nested.first );
        nest_str.push_back( nested.first );
    }
    nest_menu.query();
    const int nest_choice = nest_menu.ret;
    if( nest_choice >= 0 && nest_choice < static_cast<int>( nest_str.size() ) ) {
        const std::optional<tripoint> where = g->look_around( true );
        if( !where ) {
            return;
        }

        map &here = get_map();
        const tripoint_abs_ms abs_ms( here.getabs( *where ) );
        const tripoint_abs_omt abs_omt = project_to<coords::omt>( abs_ms );
        const tripoint_abs_sm abs_sub = project_to<coords::sm>( abs_ms );

        map target_map;
        target_map.load( abs_sub, true );
        // TODO: fix point types
        const tripoint local_ms = target_map.getlocal( abs_ms.raw() );
        mapgendata md( abs_omt, target_map, 0.0f, calendar::turn, nullptr );
        const auto &ptr = nested_mapgen[nest_str[nest_choice]].pick();
        if( ptr == nullptr ) {
            return;
        }
        ( *ptr )->nest( md, local_ms.xy() );
        target_map.save();
        g->load_npcs();
        here.invalidate_map_cache( g->get_levz() );
    }
}

static Character &pick_character( Character &preselected )
{
    std::vector< tripoint > locations;
    uilist charmenu;
    int charnum = 0;
    charmenu.addentry( charnum++, true, MENU_AUTOASSIGN, "%s", _( "You" ) );
    locations.emplace_back( g->u.pos() );
    for( const npc &guy : g->all_npcs() ) {
        charmenu.addentry( charnum++, true, MENU_AUTOASSIGN, guy.name );
        locations.emplace_back( guy.pos() );
    }
    avatar &u = get_avatar();
    u.view_offset = u.pos() - preselected.pos();
    auto iter = std::find_if( locations.begin(), locations.end(), [&preselected]( const tripoint & p ) {
        return p == preselected.pos();
    } );
    size_t preselect_index = iter != locations.end() ? std::distance( locations.begin(), iter ) : 0;

    pointmenu_cb callback( locations );
    charmenu.callback = &callback;
    charmenu.w_y_setup = 0;
    charmenu.set_selected( preselect_index );
    charmenu.query();
    if( charmenu.ret < 0 || static_cast<size_t>( charmenu.ret ) >= locations.size() ) {
        return preselected;
    }
    const size_t index = charmenu.ret;
    Character *c = g->critter_at<Character>( locations[index] );
    return c != nullptr ? *c : preselected;
}

static void control_npc_menu()
{
    std::vector<shared_ptr_fast<npc>> followers;
    uilist charmenu;
    int charnum = 0;
    for( const auto &elem : g->get_follower_list() ) {
        shared_ptr_fast<npc> follower = overmap_buffer.find_npc( elem );
        if( follower ) {
            followers.emplace_back( follower );
            charmenu.addentry( charnum++, true, MENU_AUTOASSIGN, follower->get_name() );
        }
    }
    if( followers.empty() ) {
        return;
    }
    charmenu.w_y_setup = 0;
    charmenu.query();
    if( charmenu.ret < 0 || static_cast<size_t>( charmenu.ret ) >= followers.size() ) {
        return;
    }
    get_avatar().control_npc( *followers.at( charmenu.ret ) );
}

void character_edit_menu( Character &c )
{
    npc *np = c.is_npc() ? static_cast<npc *>( &c ) : nullptr;
    player &p = static_cast<player &>( c );

    const tripoint start_view_offset = get_avatar().view_offset;

    std::string nmenu_label;
    if( np != nullptr ) {
        std::stringstream data;
        data << np->name << " " << ( np->male ? _( "Male" ) : _( "Female" ) ) << '\n';
        data << np->myclass.obj().get_name() << "; " <<
             npc_attitude_name( np->get_attitude() ) << "; " <<
             ( np->get_faction() ? np->get_faction()->name : _( "no faction" ) ) << "; " <<
             ( np->get_faction() ? np->get_faction()->currency->nname( 1 ) : _( "no currency" ) )
             << "; " <<
             "api: " << np->get_faction_ver() << '\n';
        if( np->has_destination() ) {
            data << string_format(
                     _( "Destination: %s %s" ), np->goal.to_string(),
                     overmap_buffer.ter( np->goal )->get_name() ) << '\n';
        } else {
            data << _( "No destination." ) << '\n';
        }
        data << string_format( _( "Trust: %d" ), np->op_of_u.trust ) << " "
             << string_format( _( "Fear: %d" ), np->op_of_u.fear ) << " "
             << string_format( _( "Value: %d" ), np->op_of_u.value ) << " "
             << string_format( _( "Anger: %d" ), np->op_of_u.anger ) << " "
             << string_format( _( "Owed: %d" ), np->op_of_u.owed ) << '\n';

        data << string_format( _( "Aggression: %d" ),
                               static_cast<int>( np->personality.aggression ) ) << " "
             << string_format( _( "Bravery: %d" ), static_cast<int>( np->personality.bravery ) ) << " "
             << string_format( _( "Collector: %d" ), static_cast<int>( np->personality.collector ) ) << " "
             << string_format( _( "Altruism: %d" ), static_cast<int>( np->personality.altruism ) ) << '\n';

        data << _( "Needs:" ) << '\n';
        for( const auto &need : np->needs ) {
            data << need << '\n';
        }
        data << string_format( _( "Total morale: %d" ), np->get_morale_level() ) << '\n';

        nmenu_label = data.str();
    } else {
        nmenu_label = _( "Player" );
    }

    enum edit_character {
        pick, desc, skills, stats, items, delete_items, item_worn,
        hp, stamina, morale, clear_morale, pain, needs, healthy, status, mission_add, mission_edit,
        tele, mutate, bionics, npc_class, attitude, opinion, effects,
        learn_ma, unlock_recipes, forget_items, learn_spells, level_spells
    };

    // Maybe TODO: this could actually be static if not for translations
    const std::vector<uilist_entry> static_entries = {{
            uilist_entry( edit_character::pick, true, 'p', _( "[p]ick different character" ) ),
            uilist_entry( edit_character::desc, true, 'D',  _( "Edit [D]escription - Name, Age, Height, Gender" ) ),
            uilist_entry( edit_character::skills, true, 's',  _( "Edit [s]kills" ) ),
            uilist_entry( edit_character::stats, true, 't',  _( "Edit s[t]ats" ) ),
            uilist_entry( edit_character::items, true, 'i',  _( "Grant [i]tems" ) ),
            uilist_entry( edit_character::delete_items, true, 'd',  _( "[d]elete (all) items" ) ),
            uilist_entry( edit_character::item_worn, true, 'w',  _( "[w]ear/[w]ield an item from player's inventory" ) ),
            uilist_entry( edit_character::hp, true, 'h',  _( "Set [h]it points" ) ),
            uilist_entry( edit_character::stamina, true, 'S',  _( "Set [S]tamina" ) ),
            uilist_entry( edit_character::morale, true, 'o',  _( "Set m[o]rale" ) ),
            uilist_entry( edit_character::clear_morale, true, 'O',  _( "Clear all m[O]rale effects" ) ),
            uilist_entry( edit_character::pain, true, 'P',  _( "Cause [P]ain" ) ),
            uilist_entry( edit_character::healthy, true, 'a',  _( "Set he[a]lth" ) ),
            uilist_entry( edit_character::needs, true, 'n',  _( "Set [n]eeds" ) ),
            uilist_entry( edit_character::mutate, true, 'u',  _( "M[u]tate" ) ),
            uilist_entry( edit_character::bionics, true, 'b',  _( "Edit [b]ionics" ) ),
            uilist_entry( edit_character::status, true, '@',  _( "Status Window [@]" ) ),
            uilist_entry( edit_character::tele, true, 'e',  _( "t[e]leport" ) ),
            uilist_entry( edit_character::mission_edit, true, 'M',  _( "Edit [M]issions (WARNING: Unstable!)" ) ),
            uilist_entry( edit_character::effects, true, 'E',  _( "Edit [E]ffects" ) ),
            uilist_entry( edit_character::learn_ma, true, 'l', _( "[l]earn all melee styles" ) ),
            uilist_entry( edit_character::unlock_recipes, true, 'r', _( "Unlock all [r]ecipes" ) ),
            uilist_entry( edit_character::forget_items, true, 'F', _( "[F]orget all items" ) )
        }
    };

    std::vector<uilist_entry> menu_entries = static_entries;
    if( !spell_type::get_all().empty() ) {
        menu_entries.emplace_back( edit_character::learn_spells, true, 'L', _( "[L]earn all Spells" ) );
        menu_entries.emplace_back( edit_character::level_spells, true, 'v', _( "Le[v]el a spell" ) );
    }
    if( p.is_npc() ) {
        menu_entries.emplace_back( edit_character::mission_add, true, 'm',  _( "Add [m]ission" ) );
        menu_entries.emplace_back( edit_character::npc_class, true, 'c',
                                   _( "Randomize with [c]lass" ) );
        menu_entries.emplace_back( edit_character::attitude, true, 'A',  _( "Set [A]ttitude" ) );
        menu_entries.emplace_back( edit_character::opinion, true, 'O',  _( "Set [O]pinion" ) );
    }
    uilist nmenu( nmenu_label, menu_entries );
    switch( nmenu.ret ) {
        case edit_character::pick: {
            Character &other = pick_character( c );
            get_avatar().view_offset = other.pos() - get_avatar().pos();
            // TODO: Make it not able to cause a stack overflow
            character_edit_menu( other );
            get_avatar().view_offset = start_view_offset;
        }
        return;
        case edit_character::skills:
            wishskill( &p );
            break;
        case edit_character::stats: {
            uilist smenu;
            smenu.addentry( 0, true, 'S', "%s: %d", _( "Maximum strength" ), p.str_max );
            smenu.addentry( 1, true, 'D', "%s: %d", _( "Maximum dexterity" ), p.dex_max );
            smenu.addentry( 2, true, 'I', "%s: %d", _( "Maximum intelligence" ), p.int_max );
            smenu.addentry( 3, true, 'P', "%s: %d", _( "Maximum perception" ), p.per_max );
            smenu.query();
            int *bp_ptr = nullptr;
            switch( smenu.ret ) {
                case 0:
                    bp_ptr = &p.str_max;
                    break;
                case 1:
                    bp_ptr = &p.dex_max;
                    break;
                case 2:
                    bp_ptr = &p.int_max;
                    break;
                case 3:
                    bp_ptr = &p.per_max;
                    break;
                default:
                    break;
            }

            if( bp_ptr != nullptr ) {
                int value;
                if( query_int( value, _( "Set the stat to?  Currently: %d" ), *bp_ptr ) && value >= 0 ) {
                    *bp_ptr = value;
                    p.reset_bonuses();
                    p.reset_stats();
                }
            }
        }
        break;
        case edit_character::items:
            wishitem( &p );
            break;
        case edit_character::delete_items:
            if( !query_yn( _( "Delete all items from the target?" ) ) ) {
                break;
            }
            for( auto &it : p.worn ) {
                it->on_takeoff( p );
            }
            p.worn.clear();
            p.inv_clear();
            p.remove_primary_weapon( );
            break;
        case edit_character::item_worn: {
            item *loc = game_menus::inv::titled_menu( g->u, _( "Make target equip" ) );
            if( !loc ) {
                break;
            }
            item &to_wear = *loc;
            if( to_wear.is_armor() ) {
                p.worn.push_back( to_wear.detach() );
                p.on_item_wear( to_wear );
            } else if( !to_wear.is_null() ) {
                p.set_primary_weapon( to_wear.detach() );
            }
        }
        break;
        case edit_character::hp: {
            const int torso_hp = p.get_part_hp_cur( bodypart_id( "torso" ) );
            const int head_hp = p.get_part_hp_cur( bodypart_id( "head" ) );
            const int arm_l_hp = p.get_part_hp_cur( bodypart_id( "arm_l" ) );
            const int arm_r_hp = p.get_part_hp_cur( bodypart_id( "arm_r" ) );
            const int leg_l_hp = p.get_part_hp_cur( bodypart_id( "leg_l" ) );
            const int leg_r_hp = p.get_part_hp_cur( bodypart_id( "leg_r" ) );
            uilist smenu;
            smenu.addentry( 0, true, 'q', "%s: %d", _( "Torso" ), torso_hp );
            smenu.addentry( 1, true, 'w', "%s: %d", _( "Head" ), head_hp );
            smenu.addentry( 2, true, 'a', "%s: %d", _( "Left arm" ), arm_l_hp );
            smenu.addentry( 3, true, 's', "%s: %d", _( "Right arm" ), arm_r_hp );
            smenu.addentry( 4, true, 'z', "%s: %d", _( "Left leg" ), leg_l_hp );
            smenu.addentry( 5, true, 'x', "%s: %d", _( "Right leg" ), leg_r_hp );
            smenu.addentry( 6, true, 'e', "%s: %d", _( "All" ), p.get_lowest_hp() );
            smenu.query();
            bodypart_str_id bp = bodypart_str_id( "no_a_real_part" );
            int bp_ptr = -1;
            bool all_select = false;

            switch( smenu.ret ) {
                case 0:
                    bp = bodypart_str_id( "torso" );
                    bp_ptr = torso_hp;
                    break;
                case 1:
                    bp = bodypart_str_id( "head" );
                    bp_ptr = head_hp;
                    break;
                case 2:
                    bp = bodypart_str_id( "arm_l" );
                    bp_ptr = arm_l_hp;
                    break;
                case 3:
                    bp = bodypart_str_id( "arm_r" );
                    bp_ptr = arm_r_hp;
                    break;
                case 4:
                    bp = bodypart_str_id( "leg_l" );
                    bp_ptr = leg_l_hp;
                    break;
                case 5:
                    bp = bodypart_str_id( "leg_r" );
                    bp_ptr = leg_r_hp;
                    break;
                case 6:
                    all_select = true;
                    break;
                default:
                    break;
            }

            if( bp.is_valid() ) {
                int value;
                if( query_int( value, _( "Set the hitpoints to?  Currently: %d" ), bp_ptr ) && value >= 0 ) {
                    p.set_part_hp_cur( bp.id(), value );
                    p.reset_stats();
                }
            } else if( all_select ) {
                int value;
                if( query_int( value, _( "Set the hitpoints to?  Currently: %d" ), p.get_lowest_hp() ) &&
                    value >= 0 ) {
                    p.set_all_parts_hp_cur( value );
                    p.reset_stats();
                }
            }
        }
        break;
        case edit_character::stamina:
            int value;
            if( query_int( value, _( "Set stamina to?  Current: %d. Max: %d." ), p.get_stamina(),
                           p.get_stamina_max() ) ) {
                if( value >= 0 && value <= p.get_stamina_max() ) {
                    p.set_stamina( value );
                } else {
                    add_msg( m_bad, _( "Target stamina value out of bounds!" ) );
                }
            }
            break;
        case edit_character::morale: {
            int current_morale_level = p.get_morale_level();
            int value;
            if( query_int( value, _( "Set the morale to?  Currently: %d" ), current_morale_level ) ) {
                int morale_level_delta = value - current_morale_level;
                p.add_morale( MORALE_PERM_DEBUG, morale_level_delta );
                p.apply_persistent_morale();
            }
        }
        break;
        case edit_character::clear_morale:
            p.clear_morale();
            break;
        case edit_character::opinion: {
            if( np == nullptr ) {
                // HACK: For some reason, tidy is not satisfied with simple assert(np)
                std::abort();
            }
            uilist smenu;
            smenu.addentry( 0, true, 'h', "%s: %d", _( "trust" ), np->op_of_u.trust );
            smenu.addentry( 1, true, 's', "%s: %d", _( "fear" ), np->op_of_u.fear );
            smenu.addentry( 2, true, 't', "%s: %d", _( "value" ), np->op_of_u.value );
            smenu.addentry( 3, true, 'f', "%s: %d", _( "anger" ), np->op_of_u.anger );
            smenu.addentry( 4, true, 'd', "%s: %d", _( "owed" ), np->op_of_u.owed );

            smenu.query();
            int value;
            switch( smenu.ret ) {
                case 0:
                    if( query_int( value, _( "Set trust to?  Currently: %d" ),
                                   np->op_of_u.trust ) ) {
                        np->op_of_u.trust = value;
                    }
                    break;
                case 1:
                    if( query_int( value, _( "Set fear to?  Currently: %d" ), np->op_of_u.fear ) ) {
                        np->op_of_u.fear = value;
                    }
                    break;
                case 2:
                    if( query_int( value, _( "Set value to?  Currently: %d" ),
                                   np->op_of_u.value ) ) {
                        np->op_of_u.value = value;
                    }
                    break;
                case 3:
                    if( query_int( value, _( "Set anger to?  Currently: %d" ),
                                   np->op_of_u.anger ) ) {
                        np->op_of_u.anger = value;
                    }
                    break;
                case 4:
                    if( query_int( value, _( "Set owed to?  Currently: %d" ), np->op_of_u.owed ) ) {
                        np->op_of_u.owed = value;
                    }
                    break;
            }
        }
        break;
        case edit_character::desc: {
            uilist smenu;
            smenu.text = _( "Select a value and press enter to change it." );
            if( p.is_avatar() ) {
                smenu.addentry( 0, true, 's', "%s: %s", _( "Current save file name" ), get_avatar().get_save_id() );
            }
            smenu.addentry( 1, true, 'n', "%s: %s", _( "Current pre-Cataclysm name" ), p.name );
            smenu.addentry( 2, true, 'a', "%s: %d", _( "Current age" ), p.base_age() );
            smenu.addentry( 3, true, 'h', "%s: %d", _( "Current height in cm" ), p.base_height() );
            smenu.addentry( 4, true, 'h', "%s: %s", _( "Current gender" ),
                            p.male ? _( "Male" ) : _( "Female" ) );
            smenu.query();
            switch( smenu.ret ) {
                case 0: {
                    std::string buf = get_avatar().get_save_id();
                    string_input_popup popup;
                    popup
                    .title( _( "Rename save file (WARNING: this will duplicate the save):" ) )
                    .width( 85 )
                    .edit( buf );
                    if( popup.confirmed() ) {
                        get_avatar().set_save_id( buf );
                    }
                }
                break;
                case 1: {
                    std::string buf = p.name;
                    string_input_popup popup;
                    popup
                    .title( _( "Rename character:" ) )
                    .width( 85 )
                    .edit( buf );
                    if( popup.confirmed() ) {
                        p.name = buf;
                    }
                }
                break;
                case 2: {
                    string_input_popup popup;
                    popup
                    .title( _( "Enter age in years.  Minimum 16, maximum 55" ) )
                    .text( string_format( "%d", p.base_age() ) )
                    .only_digits( true );
                    const int result = popup.query_int();
                    if( result != 0 ) {
                        p.set_base_age( clamp( result, 16, 55 ) );
                    }
                }
                break;
                case 3: {
                    string_input_popup popup;
                    popup
                    .title( _( "Enter height in centimeters.  Minimum 145, maximum 200" ) )
                    .text( string_format( "%d", p.base_height() ) )
                    .only_digits( true );
                    const int result = popup.query_int();
                    if( result != 0 ) {
                        p.set_base_height( clamp( result, 145, 200 ) );
                    }
                }
                break;
                case 4: {
                    p.male = !p.male;
                }
                break;
            }
        }
        break;
        case edit_character::pain: {
            int value;
            if( query_int( value, _( "Cause how much pain?  pain: %d" ), p.get_pain() ) ) {
                p.mod_pain( value );
            }
        }
        break;
        case edit_character::needs: {
            uilist smenu;
            smenu.addentry( 0, true, 's', "%s: %d", _( "Stored kCal" ), p.get_stored_kcal() );
            smenu.addentry( 1, true, 't', "%s: %d", _( "Thirst" ), p.get_thirst() );
            smenu.addentry( 2, true, 'f', "%s: %d", _( "Fatigue" ), p.get_fatigue() );
            smenu.addentry( 3, true, 'd', "%s: %d", _( "Sleep Deprivation" ), p.get_sleep_deprivation() );
            smenu.addentry( 4, true, 'a', _( "Reset all basic needs" ) );

            const auto &vits = vitamin::all();
            for( const auto &v : vits ) {
                smenu.addentry( -1, true, 0, "%s: %d", v.second.name(), p.vitamin_get( v.first ) );
            }

            smenu.query();
            int value;
            switch( smenu.ret ) {
                case 0:
                    if( query_int( value, _( "Set stored kCal to?  Currently: %d" ), p.get_stored_kcal() ) ) {
                        p.set_stored_kcal( value );
                    }
                    break;

                case 1:
                    if( query_int( value, _( "Set thirst to?  Currently: %d" ), p.get_thirst() ) ) {
                        p.set_thirst( value );
                    }
                    break;

                case 2:
                    if( query_int( value, _( "Set fatigue to?  Currently: %d" ), p.get_fatigue() ) ) {
                        p.set_fatigue( value );
                    }
                    break;

                case 3:
                    if( query_int( value, _( "Set sleep deprivation to?  Currently: %d" ),
                                   p.get_sleep_deprivation() ) ) {
                        p.set_sleep_deprivation( value );
                    }
                    break;
                case 4:
                    p.set_thirst( 0 );
                    p.set_fatigue( 0 );
                    p.set_sleep_deprivation( 0 );
                    p.set_stored_kcal( p.max_stored_kcal() );
                    break;
                default:
                    const int non_vitamin_entries = smenu.entries.size() - vits.size();
                    if( smenu.ret >= non_vitamin_entries &&
                        smenu.ret < static_cast<int>( vits.size() + non_vitamin_entries ) ) {
                        auto iter = std::next( vits.begin(), smenu.ret - non_vitamin_entries );
                        if( query_int( value, _( "Set %s to?  Currently: %d" ),
                                       iter->second.name(), p.vitamin_get( iter->first ) ) ) {
                            p.vitamin_set( iter->first, value );
                        }
                    }
            }

        }
        break;
        case edit_character::mutate:
            wishmutate( &p );
            break;
        case edit_character::bionics:
            wishbionics( *p.as_character() );
            break;
        case edit_character::healthy: {
            uilist smenu;
            smenu.addentry( 0, true, 'h', "%s: %d", _( "Health" ), static_cast<int>( p.get_healthy() ) );
            smenu.addentry( 1, true, 'm', "%s: %d", _( "Health modifier" ),
                            static_cast<int>( p.get_healthy_mod() ) );
            smenu.addentry( 2, true, 'r', "%s: %d", _( "Radiation" ), p.get_rad() );
            smenu.query();
            int value;
            switch( smenu.ret ) {
                case 0:
                    if( query_int( value, _( "Set the value to?  Currently: %d" ),
                                   static_cast<int>( p.get_healthy() ) ) ) {
                        p.set_healthy( value );
                    }
                    break;
                case 1:
                    if( query_int( value, _( "Set the value to?  Currently: %d" ),
                                   static_cast<int>( p.get_healthy_mod() ) ) ) {
                        p.set_healthy_mod( value );
                    }
                    break;
                case 2:
                    if( query_int( value, _( "Set the value to?  Currently: %d" ), p.get_rad() ) ) {
                        p.set_rad( value );
                    }
                    break;
                default:
                    break;
            }
        }
        break;
        case edit_character::status:
            character_display::disp_info( p );
            break;
        case edit_character::mission_add: {
            uilist types;
            types.text = _( "Choose mission type" );
            const auto all_missions = mission_type::get_all();
            std::vector<const mission_type *> mts;
            for( size_t i = 0; i < all_missions.size(); i++ ) {
                types.addentry( i, true, -1, all_missions[i].tname() );
                mts.push_back( &all_missions[i] );
            }

            types.query();
            if( types.ret >= 0 && types.ret < static_cast<int>( mts.size() ) ) {
                np->add_new_mission( mission::reserve_new( mts[types.ret]->id, np->getID() ) );
            }
        }
        break;
        case edit_character::mission_edit:
            mission_debug::edit( p );
            break;
        case edit_character::tele: {
            if( const std::optional<tripoint> newpos = g->look_around( true ) ) {
                p.setpos( *newpos );
                if( p.is_player() ) {
                    if( p.is_mounted() ) {
                        p.mounted_creature->setpos( *newpos );
                    }
                    g->update_map( g->u );
                }
            }
        }
        break;
        case edit_character::npc_class: {
            uilist classes;
            classes.text = _( "Choose new class" );
            std::vector<npc_class_id> ids;
            size_t i = 0;
            for( auto &cl : npc_class::get_all() ) {
                ids.push_back( cl.id );
                classes.addentry( i, true, -1, cl.get_name() );
                i++;
            }

            classes.query();
            if( classes.ret < static_cast<int>( ids.size() ) && classes.ret >= 0 ) {
                np->randomize( ids[classes.ret] );
            }
        }
        break;
        case edit_character::attitude: {
            uilist attitudes_ui;
            attitudes_ui.text = _( "Choose new attitude" );
            std::vector<npc_attitude> attitudes;
            for( int i = NPCATT_NULL; i < NPCATT_END; i++ ) {
                npc_attitude att_id = static_cast<npc_attitude>( i );
                std::string att_name = npc_attitude_name( att_id );
                attitudes.push_back( att_id );
                if( att_name == _( "Unknown attitude" ) ) {
                    continue;
                }

                attitudes_ui.addentry( i, true, -1, att_name );
            }

            attitudes_ui.query();
            if( attitudes_ui.ret < static_cast<int>( attitudes.size() ) && attitudes_ui.ret >= 0 ) {
                np->set_attitude( attitudes[attitudes_ui.ret] );
            }
        }
        break;
        case edit_character::learn_ma:
            add_msg( m_info, _( "Martial arts debug." ) );
            add_msg( _( "Your eyes blink rapidly as knowledge floods your brain." ) );
            for( auto &style : all_martialart_types() ) {
                if( style != matype_id( "style_none" ) ) {
                    c.martial_arts_data->add_martialart( style );
                }
            }
            add_msg( m_good, _( "You now know a lot more than just 10 styles of kung fu." ) );
            break;

        case edit_character::effects:
            effect_edit_menu( c );
            break;

        case edit_character::unlock_recipes: {
            add_msg( m_info, _( "Recipe debug." ) );
            add_msg( _( "Your eyes blink rapidly as knowledge floods your brain." ) );
            for( const auto &e : recipe_dict ) {
                p.learn_recipe( &e.second );
            }
            add_msg( m_good, _( "You know how to craft that now." ) );
        }
        break;
        case edit_character::forget_items:
            add_msg( m_info, _( "Item debug." ) );
            uistate.read_items.clear();
            add_msg( m_bad, _( "You don't know about any items anymore." ) );
            break;
        case edit_character::learn_spells:
            if( spell_type::get_all().empty() ) {
                add_msg( m_bad, _( "There are no spells to learn.  You must install a mod that adds some." ) );
            } else {
                for( const spell_type &learn : spell_type::get_all() ) {
                    c.magic->learn_spell( &learn, g->u, true );
                }
                add_msg( m_good,
                         _( "You have become an Archwizardpriest!  What will you do with your newfound power?" ) );
            }
            break;
        case edit_character::level_spells: {
            std::vector<spell *> spells = c.magic->get_spells();
            if( spells.empty() ) {
                add_msg( m_bad, _( "Try learning some spells first." ) );
                return;
            }
            std::vector<uilist_entry> uiles;
            {
                uilist_entry uile( _( "Spell" ) );
                uile.ctxt = string_format( "%s %s",
                                           //~ translation should not exceed 4 console cells
                                           right_justify( _( "LVL" ), 4 ),
                                           //~ translation should not exceed 4 console cells
                                           right_justify( _( "MAX" ), 4 ) );
                uile.enabled = false;
                uile.force_color = c_light_blue;
                uiles.emplace_back( uile );
            }
            int retval = 0;
            for( spell *sp : spells ) {
                uilist_entry uile( sp->name() );
                uile.ctxt = string_format( "%4d %4d", sp->get_level(), sp->get_max_level() );
                uile.retval = retval++;
                uile.enabled = !sp->is_max_level();
                uiles.emplace_back( uile );
            }
            int action = uilist( _( "Debug level spell:" ), uiles );
            if( action < 0 ) {
                break;
            }
            int desired_level = 0;
            int cur_level = spells[action]->get_level();
            query_int( desired_level, _( "Desired Spell Level: (Current %d)" ), cur_level );
            desired_level = std::min( desired_level, spells[action]->get_max_level() );
            while( cur_level < desired_level ) {
                spells[action]->gain_level();
                cur_level = spells[action]->get_level();
            }
            add_msg( m_good, _( "%s is now level %d!" ), spells[action]->name(), spells[action]->get_level() );
            break;
        }
        case UILIST_CANCEL:
            return;
    }

    // TODO: Stack overflow possibility
    character_edit_menu( c );
}

static std::string mission_status_string( mission::mission_status status )
{
    static const std::map<mission::mission_status, std::string> desc{ {
            { mission::mission_status::yet_to_start, translate_marker( "Yet to start" ) },
            { mission::mission_status::in_progress, translate_marker( "In progress" ) },
            { mission::mission_status::success, translate_marker( "Success" ) },
            { mission::mission_status::failure, translate_marker( "Failure" ) }
        }
    };

    const auto &iter = desc.find( status );
    if( iter != desc.end() ) {
        return _( iter->second );
    }

    return _( "Bugged" );
}

std::string mission_debug::describe( const mission &m )
{
    std::stringstream data;
    data << _( "Type:" ) << m.type->id.str();
    data << _( " Status:" ) << mission_status_string( m.status );
    data << _( " ID:" ) << m.uid;
    data << _( " NPC ID:" ) << m.npc_id;
    data << _( " Target:" ) << m.target.to_string();
    data << _( "Player ID:" ) << m.player_id;

    return data.str();
}

static void add_header( uilist &mmenu, const std::string &str )
{
    if( !mmenu.entries.empty() ) {
        mmenu.addentry( -1, false, -1, "" );
    }
    uilist_entry header( -1, false, -1, str, c_yellow, c_yellow );
    header.force_color = true;
    mmenu.entries.push_back( header );
}

void mission_debug::edit( Character &who )
{
    if( who.is_player() ) {
        edit_player();
    } else if( who.is_npc() ) {
        edit_npc( dynamic_cast<npc &>( who ) );
    }
}

void mission_debug::edit_npc( npc &who )
{
    npc_chatbin &bin = who.chatbin;
    std::vector<mission *> all_missions;

    uilist mmenu;
    mmenu.text = _( "Select mission to edit" );

    add_header( mmenu, _( "Currently assigned missions:" ) );
    for( mission *m : bin.missions_assigned ) {
        mmenu.addentry( all_missions.size(), true, MENU_AUTOASSIGN, "%s", m->type->id.c_str() );
        all_missions.emplace_back( m );
    }

    add_header( mmenu, _( "Not assigned missions:" ) );
    for( mission *m : bin.missions ) {
        mmenu.addentry( all_missions.size(), true, MENU_AUTOASSIGN, "%s", m->type->id.c_str() );
        all_missions.emplace_back( m );
    }

    mmenu.query();
    if( mmenu.ret < 0 || mmenu.ret >= static_cast<int>( all_missions.size() ) ) {
        return;
    }

    edit_mission( *all_missions[mmenu.ret] );
}

void mission_debug::edit_player()
{
    std::vector<mission *> all_missions;

    uilist mmenu;
    mmenu.text = _( "Select mission to edit" );

    add_header( mmenu, _( "Active missions:" ) );
    for( mission *m : g->u.active_missions ) {
        mmenu.addentry( all_missions.size(), true, MENU_AUTOASSIGN, "%s", m->type->id.c_str() );
        all_missions.emplace_back( m );
    }

    add_header( mmenu, _( "Completed missions:" ) );
    for( mission *m : g->u.completed_missions ) {
        mmenu.addentry( all_missions.size(), true, MENU_AUTOASSIGN, "%s", m->type->id.c_str() );
        all_missions.emplace_back( m );
    }

    add_header( mmenu, _( "Failed missions:" ) );
    for( mission *m : g->u.failed_missions ) {
        mmenu.addentry( all_missions.size(), true, MENU_AUTOASSIGN, "%s", m->type->id.c_str() );
        all_missions.emplace_back( m );
    }

    mmenu.query();
    if( mmenu.ret < 0 || mmenu.ret >= static_cast<int>( all_missions.size() ) ) {
        return;
    }

    edit_mission( *all_missions[mmenu.ret] );
}

static bool remove_from_vec( std::vector<mission *> &vec, mission *m )
{
    auto iter = std::remove( vec.begin(), vec.end(), m );
    bool ret = iter != vec.end();
    vec.erase( iter, vec.end() );
    return ret;
}

void mission_debug::remove_mission( mission &m )
{
    if( remove_from_vec( g->u.active_missions, &m ) ) {
        add_msg( _( "Removing from active_missions" ) );
    }
    if( remove_from_vec( g->u.completed_missions, &m ) ) {
        add_msg( _( "Removing from completed_missions" ) );
    }
    if( remove_from_vec( g->u.failed_missions, &m ) ) {
        add_msg( _( "Removing from failed_missions" ) );
    }

    if( g->u.active_mission == &m ) {
        g->u.active_mission = nullptr;
        add_msg( _( "Unsetting active mission" ) );
    }

    const auto giver = g->find_npc( m.npc_id );
    if( giver != nullptr ) {
        if( remove_from_vec( giver->chatbin.missions_assigned, &m ) ) {
            add_msg( _( "Removing from %s missions_assigned" ), giver->name );
        }
        if( remove_from_vec( giver->chatbin.missions, &m ) ) {
            add_msg( _( "Removing from %s missions" ), giver->name );
        }
    }
}

void mission_debug::edit_mission( mission &m )
{
    uilist mmenu;
    mmenu.text = describe( m );

    enum {
        M_FAIL, M_SUCCEED, M_REMOVE
    };

    mmenu.addentry( M_FAIL, true, 'f', "%s", _( "Fail mission" ) );
    mmenu.addentry( M_SUCCEED, true, 'c', "%s", _( "Mark as complete" ) );
    mmenu.addentry( M_REMOVE, true, 'r', "%s", _( "Remove mission without proper cleanup" ) );

    mmenu.query();
    switch( mmenu.ret ) {
        case M_FAIL:
            m.fail();
            break;
        case M_SUCCEED:
            m.status = mission::mission_status::success;
            break;
        case M_REMOVE:
            remove_mission( m );
            break;
    }
}

void benchmark( const int max_difference, bench_kind kind )
{
    std::string bench_name;
    switch( kind ) {
        case bench_kind::FPS:
            bench_name = _( "FPS benchmark" );
            break;
        case bench_kind::DRAW:
            bench_name = _( "Draw benchmark" );
            break;
    }

    static_popup popup;
    //~ %s is benchmark name
    popup.on_top( true ).message( _( "%s in progress…" ), bench_name );
    ui_manager::redraw();
    refresh_display(); // Show the popup

    // call the draw procedure as many times as possible in max_difference milliseconds
    auto start_tick = std::chrono::steady_clock::now();
    auto end_tick = start_tick;
    int64_t difference = 0;
    int draw_counter = 0;

    while( true ) {
        end_tick = std::chrono::steady_clock::now();
        difference = std::chrono::duration_cast<std::chrono::milliseconds>( end_tick - start_tick ).count();
        if( difference >= max_difference ) {
            break;
        }
        g->invalidate_main_ui_adaptor();
        inp_mngr.pump_events();
        ui_manager::redraw_invalidated();
        if( kind == bench_kind::FPS ) {
            refresh_display();
        }
        draw_counter++;
    }

    DebugLog( DL::Info, DC::Main ) << bench_name << ":\n" <<
                                   "\n| USE_TILES |  RENDERER | FRAMEBUFFER_ACCEL | USE_COLOR_MODULATED_TEXTURES | FPS |" <<
                                   "\n|:---:|:---:|:---:|:---:|:---:|\n| " <<
                                   get_option<bool>( "USE_TILES" ) << " | " <<
#if !defined(__ANDROID__)
                                   get_option<std::string>( "RENDERER" ) << " | " <<
#else
                                   get_option<bool>( "SOFTWARE_RENDERING" ) << " | " <<
#endif
                                   get_option<bool>( "FRAMEBUFFER_ACCEL" ) << " | " <<
                                   get_option<bool>( "USE_COLOR_MODULATED_TEXTURES" ) << " | " <<
                                   static_cast<int>( 1000.0 * draw_counter / static_cast<double>( difference ) ) << " |\n";

    std::string msg_txt;
    switch( kind ) {
        case bench_kind::FPS:
            //~ 'Refresh' here means draw + display, i.e. includes OS display delay.
            //~ This is the actual "FPS" people often measure in games.
            msg_txt = _( "Refreshed %1$d times in %2$.3f seconds.  (%3$.3f fps average)" );
            break;
        case bench_kind::DRAW:
            //~ 'Draw' here means time taken to draw the scene without displaying it.
            //~ It's a separate thing from "FPS", and is mostly useful for profiling
            //~ draw calls and measuring OS display delay caused by UI sync.
            msg_txt = _( "Drew %1$d times in %2$.3f seconds.  (%3$.3f per second average)" );
            break;
    }
    add_msg( m_info, msg_txt, draw_counter,
             difference / 1000.0, 1000.0 * draw_counter / static_cast<double>( difference ) );
}

// prompts player to select 2 points that will form a rectangular area
static std::optional<tripoint_range<tripoint>> select_area()
{
    static_popup popup;
    popup.on_top( true );
    popup.message( "%s", _( "Select first point." ) );

    tripoint initial_pos = g->u.pos();
    const look_around_result first = g->look_around( false, initial_pos, initial_pos,
                                     false, true, false, false, tripoint_zero, true );

    if( !first.position ) {
        return std::nullopt;
    }

    popup.message( "%s", _( "Select second point." ) );
    const look_around_result second = g->look_around( false, initial_pos, *first.position,
                                      true, true, false, false, tripoint_zero, true );

    if( !second.position ) {
        return std::nullopt;
    }

    return get_map().points_in_rectangle(
               first.position.value(), second.position.value() );
}

void debug()
{
    bool debug_menu_has_hotkey = hotkey_for_action( ACTION_DEBUG, false ) != -1;
    int action = debug_menu_uilist( debug_menu_has_hotkey );
    avatar &u = g->u;
    map &m = g->m;
    switch( action ) {
        case DEBUG_WISH:
            debug_menu::wishitem( &u );
            break;

        case DEBUG_SHORT_TELEPORT:
            debug_menu::teleport_short();
            break;

        case DEBUG_LONG_TELEPORT:
            debug_menu::teleport_long();
            break;

        case DEBUG_TELEPORT_TO_OVERMAP_SPECIAL:
            debug_menu::teleport_to_overmap_special();
            break;

        case DEBUG_REVEAL_MAP: {
            auto &cur_om = g->get_cur_om();
            for( int i = 0; i < OMAPX; i++ ) {
                for( int j = 0; j < OMAPY; j++ ) {
                    for( int k = -OVERMAP_DEPTH; k <= OVERMAP_HEIGHT; k++ ) {
                        cur_om.seen( { i, j, k } ) = true;
                    }
                }
            }
            add_msg( m_good, _( "Current overmap revealed." ) );
        }
        break;

        case DEBUG_SPAWN_NPC: {
            shared_ptr_fast<npc> temp = make_shared_fast<npc>();
            temp->randomize();
            temp->spawn_at_precise( { g->get_levx(), g->get_levy() }, u.pos() + point( -4, -4 ) );
            overmap_buffer.insert_npc( temp );
            temp->form_opinion( u );
            temp->mission = NPC_MISSION_NULL;
            temp->add_new_mission( mission::reserve_random( ORIGIN_ANY_NPC, temp->global_omt_location(),
                                   temp->getID() ) );
            std::string new_fac_id = "solo_";
            new_fac_id += temp->name;
            // create a new "lone wolf" faction for this one NPC
            faction *new_solo_fac = g->faction_manager_ptr->add_new_faction( temp->name,
                                    faction_id( new_fac_id ), faction_id( "no_faction" ) );
            temp->set_fac( new_solo_fac ? new_solo_fac->id : faction_id( "no_faction" ) );
            g->load_npcs();
        }
        break;

        case DEBUG_SPAWN_MON:
            debug_menu::wishmonster( std::nullopt );
            break;

        case DEBUG_GAME_STATE: {
            std::string mfus;
            std::vector<std::pair<m_flag, int>> sorted;
            sorted.reserve( m_flag::MF_MAX );
            for( int f = 0; f < m_flag::MF_MAX; f++ ) {
                sorted.emplace_back( static_cast<m_flag>( f ),
                                     MonsterGenerator::generator().m_flag_usage_stats[f] );
            }
            std::sort( sorted.begin(), sorted.end(), []( std::pair<m_flag, int> a, std::pair<m_flag, int> b ) {
                return a.second != b.second ? a.second > b.second : a.first < b.first;
            } );
            for( auto &m_flag_stat : sorted ) {
                mfus += string_format( "%s;%d\n", io::enum_to_string<m_flag>( m_flag_stat.first ),
                                       m_flag_stat.second );
            }
            DebugLog( DL::Info, DC::Main ) << "Monster flag usage statistics:\nFLAG;COUNT\n" << mfus;
            std::fill( MonsterGenerator::generator().m_flag_usage_stats.begin(),
                       MonsterGenerator::generator().m_flag_usage_stats.end(), 0 );
            popup_top( "Monster flag usage statistics were dumped to debug.log and cleared." );

            std::string s = _( "Location %d:%d in %d:%d, %s\n" );
            s += _( "Current turn: %d.\n%s\n" );
            s += vgettext( "%d creature exists.\n", "%d creatures exist.\n", g->num_creatures() );
            popup_top(
                s.c_str(),
                u.posx(), g->u.posy(), g->get_levx(), g->get_levy(),
                overmap_buffer.ter( g->u.global_omt_location() )->get_name(),
                to_turns<int>( calendar::turn - calendar::turn_zero ),
                get_option<bool>( "RANDOM_NPC" ) ? _( "NPCs are going to spawn." ) :
                _( "NPCs are NOT going to spawn." ),
                g->num_creatures() );
            for( const npc &guy : g->all_npcs() ) {
                tripoint t = guy.global_sm_location();
                add_msg( m_info, _( "%s: map ( %d:%d ) pos ( %d:%d )" ), guy.name, t.x,
                         t.y, guy.posx(), guy.posy() );
            }

            add_msg( m_info, _( "(you: %d:%d)" ), u.posx(), u.posy() );
            add_msg( m_info, _( "Thirst: %d, kCal: %d / %d" ), u.get_thirst(), u.get_stored_kcal(),
                     u.max_stored_kcal() );
            add_msg( m_info, _( "Body Mass Index: %.0f\nBasal Metabolic Rate: %i" ), u.bmi(), u.bmr() );
            if( get_option<bool>( "STATS_THROUGH_KILLS" ) ) {
                add_msg( m_info, _( "Kill xp: %d" ), u.kill_xp() );
            }
            g->invalidate_main_ui_adaptor();
            g->disp_NPCs();
            break;
        }
        case DEBUG_REPRODUCE_AREA: {
            const std::optional<tripoint_range<tripoint>> points_opt = select_area();
            if( !points_opt.has_value() ) {
                break;
            }

            const tripoint_range<tripoint> points = points_opt.value();
            std::vector<Creature *> creatures = g->get_creatures_if(
            [&points]( const Creature & critter ) -> bool {
                return !critter.is_avatar() && critter.is_monster() && points.is_point_inside( critter.pos() );
            } );

            for( Creature *critter : creatures ) {
                static_cast<monster *>( critter )->reproduce();
            }
        }
        break;
        case DEBUG_ERASE_ITEMS_AREA: {
            const std::optional<tripoint_range<tripoint>> points_opt = select_area();
            if( !points_opt.has_value() ) {
                break;
            }

            const tripoint_range<tripoint> points = points_opt.value();

            const int count = std::accumulate( points.begin(), points.end(), 0,
            [&m]( int sum, const tripoint & p ) {
                const int size = m.i_at( p ).size();
                m.i_clear( p );
                return sum + size;
            } );
            add_msg( m_good, string_format( _( "Erased %d items." ), count ) );
        }
        break;
        case DEBUG_KILL_AREA: {
            const std::optional<tripoint_range<tripoint>> points_opt = select_area();
            if( !points_opt.has_value() ) {
                break;
            }

            const tripoint_range<tripoint> points = points_opt.value();
            std::vector<Creature *> creatures = g->get_creatures_if(
            [&points]( const Creature & critter ) -> bool {
                return !critter.is_avatar() && points.is_point_inside( critter.pos() );
            } );

            for( Creature *critter : creatures ) {
                critter->die( nullptr );
            }

            g->cleanup_dead();
        }
        break;
        case DEBUG_KILL_NPCS:
            for( npc &guy : g->all_npcs() ) {
                add_msg( _( "%s's head implodes!" ), guy.name );
                guy.set_part_hp_cur( bodypart_id( "head" ), 0 );
            }
            break;

        case DEBUG_MUTATE:
            debug_menu::wishmutate( &u );
            break;

        case DEBUG_SPAWN_VEHICLE:
            if( m.veh_at( u.pos() ) ) {
                add_msg( m_bad, "There's already vehicle here." );
            } else {
                // Vector of name, id so that we can sort by name
                std::vector<std::pair<std::string, vproto_id>> veh_strings;
                for( auto &elem : vehicle_prototype::get_all() ) {
                    if( elem == vproto_id( "custom" ) ) {
                        continue;
                    }
                    veh_strings.emplace_back( _( elem->name ), elem );
                }
                std::sort( veh_strings.begin(), veh_strings.end(), localized_compare );
                uilist veh_menu;
                veh_menu.text = _( "Choose vehicle to spawn" );
                int menu_ind = 0;
                for( auto &elem : veh_strings ) {
                    //~ Menu entry in vehicle wish menu: 1st string: displayed name, 2nd string: internal name of vehicle
                    veh_menu.addentry( menu_ind, true, MENU_AUTOASSIGN, _( "%1$s (%2$s)" ),
                                       elem.first, elem.second.c_str() );
                    ++menu_ind;
                }
                veh_menu.query();
                if( veh_menu.ret >= 0 && veh_menu.ret < static_cast<int>( veh_strings.size() ) ) {
                    // Didn't cancel
                    const vproto_id &selected_opt = veh_strings[veh_menu.ret].second;
                    tripoint dest = u.pos();
                    uilist veh_cond_menu;
                    veh_cond_menu.text = _( "Vehicle condition" );
                    veh_cond_menu.addentry( 0, true, MENU_AUTOASSIGN, _( "Light damage" ) );
                    veh_cond_menu.addentry( 1, true, MENU_AUTOASSIGN, _( "Undamaged" ) );
                    veh_cond_menu.addentry( 2, true, MENU_AUTOASSIGN, _( "Disabled (tires or engine)" ) );
                    veh_cond_menu.query();

                    if( veh_cond_menu.ret >= 0 && veh_cond_menu.ret < 3 ) {
                        int dir = 0;
                        if( query_int( dir, -90, _( "Vehicle direction (in degrees): " ) ) ) {
                            vehicle *veh = m.add_vehicle( selected_opt, dest,
                                                          normalize( units::from_degrees( dir ) ),
                                                          100, veh_cond_menu.ret - 1,
                                                          true,
                                                          false,
                                                          true );
                            if( veh != nullptr ) {
                                veh->set_owner( u );
                                m.board_vehicle( dest, &u );
                            }
                        }
                    }
                }
            }
            break;

        case DEBUG_SPAWN_ARTIFACT:
            if( const std::optional<tripoint> center = g->look_around( true ) ) {
                artifact_natural_property prop = static_cast<artifact_natural_property>( rng( ARTPROP_NULL + 1,
                                                 ARTPROP_MAX - 1 ) );
                m.create_anomaly( *center, prop );
                m.spawn_natural_artifact( *center, prop );
            }
            break;

        case DEBUG_SPAWN_CLAIRVOYANCE:
            u.i_add( item::spawn( "architect_cube", calendar::turn ) );
            break;

        case DEBUG_MAP_EDITOR:
            g->look_debug();
            break;

        case DEBUG_CHANGE_WEATHER: {
            weather_manager &weather = get_weather();
            uilist weather_menu;
            weather_menu.text = _( "Select new weather pattern:" );
            weather_menu.addentry( 0, true, MENU_AUTOASSIGN, !weather.weather_override ?
                                   _( "Keep normal weather patterns" ) : _( "Disable weather forcing" ) );
            for( size_t i = 0; i < weather_types::get_all().size(); i++ ) {
                weather_menu.addentry( i, true, MENU_AUTOASSIGN,
                                       weather_types::get_all()[i].name.translated() );
            }

            weather_menu.query();

            if( weather_menu.ret >= 0 &&
                static_cast<size_t>( weather_menu.ret ) < weather_types::get_all().size() ) {
                const weather_type_id selected_weather = weather_types::get_all()[weather_menu.ret].id;
                weather.weather_override = selected_weather;
                weather.set_nextweather( calendar::turn );
            }
        }
        break;

        case DEBUG_WIND_DIRECTION: {
            weather_manager &weather = get_weather();
            uilist wind_direction_menu;
            wind_direction_menu.text = _( "Select new wind direction:" );
            wind_direction_menu.addentry( 0, true, MENU_AUTOASSIGN, weather.wind_direction_override ?
                                          _( "Disable direction forcing" ) : _( "Keep normal wind direction" ) );
            int count = 1;
            for( int angle = 0; angle <= 315; angle += 45 ) {
                wind_direction_menu.addentry( count, true, MENU_AUTOASSIGN, get_wind_arrow( angle ) );
                count += 1;
            }
            wind_direction_menu.query();
            if( wind_direction_menu.ret == 0 ) {
                weather.wind_direction_override = std::nullopt;
            } else if( wind_direction_menu.ret >= 0 && wind_direction_menu.ret < 9 ) {
                weather.wind_direction_override = ( wind_direction_menu.ret - 1 ) * 45;
                weather.set_nextweather( calendar::turn );
            }
        }
        break;

        case DEBUG_WIND_SPEED: {
            weather_manager &weather = get_weather();
            uilist wind_speed_menu;
            wind_speed_menu.text = _( "Select new wind speed:" );
            wind_speed_menu.addentry( 0, true, MENU_AUTOASSIGN, weather.wind_direction_override ?
                                      _( "Disable speed forcing" ) : _( "Keep normal wind speed" ) );
            int count = 1;
            for( int speed = 0; speed <= 100; speed += 10 ) {
                std::string speedstring = std::to_string( speed ) + " " + velocity_units( VU_WIND );
                wind_speed_menu.addentry( count, true, MENU_AUTOASSIGN, speedstring );
                count += 1;
            }
            wind_speed_menu.query();
            if( wind_speed_menu.ret == 0 ) {
                weather.windspeed_override = std::nullopt;
            } else if( wind_speed_menu.ret >= 0 && wind_speed_menu.ret < 12 ) {
                int selected_wind_speed = ( wind_speed_menu.ret - 1 ) * 10;
                weather.windspeed_override = selected_wind_speed;
                weather.set_nextweather( calendar::turn );
            }
        }
        break;

        case DEBUG_GEN_SOUND: {
            const std::optional<tripoint> where = g->look_around( true );
            if( !where ) {
                return;
            }

            int volume;
            if( !query_int( volume, _( "Volume of sound: " ) ) ) {
                return;
            }

            if( volume < 0 ) {
                return;
            }

            sounds::sound( *where, volume, sounds::sound_t::order, string_format( _( "DEBUG SOUND ( %d )" ),
                           volume ) );
        }
        break;

        case DEBUG_KILL_MONS: {
            for( monster &critter : g->all_monsters() ) {
                // Use the normal death functions, useful for testing death
                // and for getting a corpse.
                if( critter.type->id != mon_generator ) {
                    critter.die( nullptr );
                }
            }
            g->cleanup_dead();
        }
        break;
        case DEBUG_DISPLAY_HORDES:
            ui::omap::display_hordes();
            break;
        case DEBUG_TEST_IT_GROUP: {
            item_group::debug_spawn();
        }
        break;

        // Damage Self
        case DEBUG_DAMAGE_SELF: {
            const int torso_hp = u.get_part_hp_cur( bodypart_id( "torso" ) );
            const int head_hp = u.get_part_hp_cur( bodypart_id( "head" ) );
            const int arm_l_hp = u.get_part_hp_cur( bodypart_id( "arm_l" ) );
            const int arm_r_hp = u.get_part_hp_cur( bodypart_id( "arm_r" ) );
            const int leg_l_hp = u.get_part_hp_cur( bodypart_id( "leg_l" ) );
            const int leg_r_hp = u.get_part_hp_cur( bodypart_id( "leg_r" ) );
            uilist smenu;
            smenu.addentry( 0, true, 'q', "%s: %d", _( "Torso" ), torso_hp );
            smenu.addentry( 1, true, 'w', "%s: %d", _( "Head" ), head_hp );
            smenu.addentry( 2, true, 'a', "%s: %d", _( "Left arm" ), arm_l_hp );
            smenu.addentry( 3, true, 's', "%s: %d", _( "Right arm" ), arm_r_hp );
            smenu.addentry( 4, true, 'z', "%s: %d", _( "Left leg" ), leg_l_hp );
            smenu.addentry( 5, true, 'x', "%s: %d", _( "Right leg" ), leg_r_hp );
            smenu.query();
            bodypart_id part;
            int dbg_damage;
            switch( smenu.ret ) {
                case 0:
                    part = bodypart_id( "torso" );
                    break;
                case 1:
                    part = bodypart_id( "head" );
                    break;
                case 2:
                    part = bodypart_id( "arm_l" );
                    break;
                case 3:
                    part = bodypart_id( "arm_r" );
                    break;
                case 4:
                    part = bodypart_id( "leg_l" );
                    break;
                case 5:
                    part = bodypart_id( "leg_r" );
                    break;
                default:
                    break;
            }
            if( query_int( dbg_damage, _( "Damage self for how much?  hp: %s" ), part.id().c_str() ) ) {
                u.apply_damage( nullptr, part, dbg_damage );
                u.die( nullptr );
            }
        }
        break;

        case DEBUG_SHOW_SOUND: {
#if defined(TILES)
            const auto &sounds_to_draw = sounds::get_monster_sounds();

            shared_ptr_fast<game::draw_callback_t> sound_cb = make_shared_fast<game::draw_callback_t>( [&]() {
                const point offset {
                    u.view_offset.xy() + point( POSX - u.posx(), POSY - u.posy() )
                };
                for( const auto &sound : sounds_to_draw.first ) {
                    mvwputch( g->w_terrain, offset + sound.xy(), c_yellow, '?' );
                }
                for( const auto &sound : sounds_to_draw.second ) {
                    mvwputch( g->w_terrain, offset + sound.xy(), c_red, '?' );
                }
            } );
            g->add_draw_callback( sound_cb );

            ui_manager::redraw();
            inp_mngr.wait_for_any_key();
#else
            popup( _( "This binary was not compiled with tiles support." ) );
#endif
        }
        break;

        case DEBUG_DISPLAY_WEATHER:
            ui::omap::display_weather();
            break;
        case DEBUG_DISPLAY_SCENTS:
            ui::omap::display_scents();
            break;
        case DEBUG_DISPLAY_DISTRIBUTION_GRIDS:
            ui::omap::display_distribution_grids();
            break;
        case DEBUG_DISPLAY_SCENTS_LOCAL:
            g->display_toggle_overlay( ACTION_DISPLAY_SCENT );
            break;
        case DEBUG_DISPLAY_SCENTS_TYPE_LOCAL:
            g->display_toggle_overlay( ACTION_DISPLAY_SCENT_TYPE );
            break;
        case DEBUG_DISPLAY_TEMP:
            g->display_toggle_overlay( ACTION_DISPLAY_TEMPERATURE );
            break;
        case DEBUG_DISPLAY_VEHICLE_AI:
            g->display_toggle_overlay( ACTION_DISPLAY_VEHICLE_AI );
            break;
        case DEBUG_DISPLAY_VISIBILITY:
            g->display_toggle_overlay( ACTION_DISPLAY_VISIBILITY );
            break;
        case DEBUG_DISPLAY_LIGHTING:
            g->display_toggle_overlay( ACTION_DISPLAY_LIGHTING );
            break;
        case DEBUG_DISPLAY_RADIATION:
            g->display_toggle_overlay( ACTION_DISPLAY_RADIATION );
            break;
        case DEBUG_DISPLAY_TRANSPARENCY:
            g->display_toggle_overlay( ACTION_DISPLAY_TRANSPARENCY );
            break;
        case DEBUG_DISPLAY_SUBMAP_GRID:
            g->debug_submap_grid_overlay = !g->debug_submap_grid_overlay;
            break;
        case ACTION_TOGGLE_ZONE_OVERLAY:
            g->show_zone_overlay = !g->show_zone_overlay;
            break;
        case DEBUG_HOUR_TIMER:
            g->toggle_debug_hour_timer();
            break;
        case DEBUG_SWAP_CHAR:
            control_npc_menu();
            break;
        case DEBUG_CHANGE_TIME: {
            auto set_turn = [&]( const int initial, const time_duration & factor, const char *const msg ) {
                const auto text = string_input_popup()
                                  .title( msg )
                                  .width( 20 )
                                  .text( std::to_string( initial ) )
                                  .only_digits( true )
                                  .query_string();
                if( text.empty() ) {
                    return;
                }
                const int new_value = std::atoi( text.c_str() );
                const time_duration offset = ( new_value - initial ) * factor;
                // Arbitrary maximal value.
                const time_point max = calendar::turn_zero + time_duration::from_turns(
                                           std::numeric_limits<int>::max() / 2 );
                calendar::turn = std::max( std::min( max, calendar::turn + offset ), calendar::turn_zero );
            };

            uilist smenu;
            static const auto years = []( const time_point & p ) {
                return static_cast<int>( ( p - calendar::turn_zero ) / calendar::year_length() );
            };
            do {
                const int iSel = smenu.ret;
                smenu.reset();
                smenu.addentry( 0, true, 'y', "%s: %d", _( "year" ), years( calendar::turn ) );
                smenu.addentry( 1, true, 's', "%s: %d", _( "season" ),
                                static_cast<int>( season_of_year( calendar::turn ) ) );
                smenu.addentry( 2, true, 'd', "%s: %d", _( "day" ), day_of_season<int>( calendar::turn ) );
                smenu.addentry( 3, true, 'h', "%s: %d", _( "hour" ), hour_of_day<int>( calendar::turn ) );
                smenu.addentry( 4, true, 'm', "%s: %d", _( "minute" ), minute_of_hour<int>( calendar::turn ) );
                smenu.addentry( 5, true, 't', "%s: %d", pgettext( "point in time", "turn" ),
                                to_turns<int>( calendar::turn - calendar::turn_zero ) );
                smenu.selected = iSel;
                smenu.query();

                switch( smenu.ret ) {
                    case 0:
                        set_turn( years( calendar::turn ), calendar::year_length(), _( "Set year to?" ) );
                        break;
                    case 1:
                        set_turn( static_cast<int>( season_of_year( calendar::turn ) ), calendar::season_length(),
                                  calendar::eternal_season() ?
                                  _( "Set eternal season to?  Warning: modifies initial season!" ) :
                                  _( "Set season to?  (0 = spring)" ) );
                        if( calendar::eternal_season() ) {
                            // Can't use season_of_year because it checks for eternal
                            season_type new_initial_season = static_cast<season_type>(
                                                                 to_turn<int>( calendar::turn ) / to_turns<int>( calendar::season_length() ) % 4
                                                             );
                            season_of_year( calendar::before_time_starts );
                            calendar::set_calendar_config( {
                                calendar::config.start_of_cataclysm(),
                                calendar::config.start_of_game(),
                                new_initial_season,
                                calendar::config.eternal_season()
                            } );
                        }
                        break;
                    case 2:
                        set_turn( day_of_season<int>( calendar::turn ), 1_days, _( "Set days to?" ) );
                        break;
                    case 3:
                        set_turn( hour_of_day<int>( calendar::turn ), 1_hours, _( "Set hour to?" ) );
                        break;
                    case 4:
                        set_turn( minute_of_hour<int>( calendar::turn ), 1_minutes, _( "Set minute to?" ) );
                        break;
                    case 5:
                        set_turn( to_turns<int>( calendar::turn - calendar::turn_zero ), 1_turns,
                                  string_format( _( "Set turn to?  (One day is %i turns)" ), to_turns<int>( 1_days ) ).c_str() );
                        break;
                    default:
                        break;
                }
            } while( smenu.ret != UILIST_CANCEL );
        }
        break;
        case DEBUG_SHOW_MUT_CAT:
            for( const auto &elem : u.mutation_category_level ) {
                add_msg( "%s: %d", elem.first.c_str(), elem.second );
            }
            break;

        case DEBUG_SHOW_MUT_CHANCES:
            for( const auto &elem : u.mutation_chances() ) {
                add_msg( "%s: %.2f", elem.first.c_str(), elem.second );
            }
            break;

        case DEBUG_OM_EDITOR:
            ui::omap::display_editor();
            break;

        case DEBUG_BENCHMARK:
        case DEBUG_BENCHMARK_FPS: {
            bench_kind kind;
            switch( action ) {
                case DEBUG_BENCHMARK:
                    kind = bench_kind::DRAW;
                    break;
                case DEBUG_BENCHMARK_FPS:
                    kind = bench_kind::FPS;
                    break;
                default:
                    debugmsg( "Not implemented" );
                    return;
            }
            const int ms = string_input_popup()
                           .title( _( "Enter benchmark length (in milliseconds):" ) )
                           .width( 20 )
                           .text( "5000" )
                           .query_int();
            debug_menu::benchmark( ms, kind );
        }
        break;

        case DEBUG_OM_TELEPORT:
            debug_menu::teleport_overmap();
            break;
        case DEBUG_OM_TELEPORT_COORDINATES:
            debug_menu::teleport_overmap( true );
            break;
        case DEBUG_TRAIT_GROUP:
            trait_group::debug_spawn();
            break;
        case DEBUG_SHOW_MSG:
            debugmsg( "Test debugmsg" );
            break;
        case DEBUG_CRASH_GAME:
            raise( SIGSEGV );
            break;
        case DEBUG_RELOAD_TRANSLATIONS:
            l10n_data::reload_catalogues();
            break;
        case DEBUG_MAP_EXTRA: {
            const std::vector<std::string> &mx_str = MapExtras::get_all_function_names();
            uilist mx_menu;
            for( const std::string &extra : mx_str ) {
                mx_menu.addentry( -1, true, -1, extra );
            }
            mx_menu.query();
            int mx_choice = mx_menu.ret;
            if( mx_choice >= 0 && mx_choice < static_cast<int>( mx_str.size() ) ) {
                const tripoint_abs_omt where_omt( ui::omap::choose_point() );
                if( where_omt != overmap::invalid_tripoint ) {
                    tripoint_abs_sm where_sm = project_to<coords::sm>( where_omt );
                    tinymap mx_map;
                    // TODO: fix point types
                    mx_map.load( where_sm.raw(), false );
                    MapExtras::apply_function( mx_str[mx_choice], mx_map, where_sm.raw() );
                    g->load_npcs();
                    m.invalidate_map_cache( g->get_levz() );
                }
            }
            break;
        }
        case DEBUG_NESTED_MAPGEN:
            debug_menu::spawn_nested_mapgen();
            break;
        case DEBUG_DISPLAY_NPC_PATH:
            g->debug_pathfinding = !g->debug_pathfinding;
            break;
        case DEBUG_PRINT_FACTION_INFO: {
            int count = 0;
            for( const auto &elem : g->faction_manager_ptr->all() ) {
                std::cout << std::to_string( count ) << " Faction_id key in factions map = " << elem.first.str() <<
                          '\n';
                std::cout << std::to_string( count ) << " Faction name associated with this id is " <<
                          elem.second.name << '\n';
                std::cout << std::to_string( count ) << " the id of that faction object is " << elem.second.id.str()
                          << '\n';
                count++;
            }
            std::cout << "Player faction is " << g->u.get_faction()->id.str() << '\n';
            break;
        }
        case DEBUG_PRINT_NPC_MAGIC: {
            for( npc &guy : g->all_npcs() ) {
                const std::vector<spell_id> spells = guy.magic->spells();
                if( spells.empty() ) {
                    std::cout << guy.disp_name() << " does not know any spells." << '\n';
                    continue;
                }
                std::cout << guy.disp_name() << "knows : ";
                int counter = 1;
                for( const spell_id &sp : spells ) {
                    std::cout << sp->name.translated() << " ";
                    if( counter < static_cast<int>( spells.size() ) ) {
                        std::cout << "and ";
                    } else {
                        std::cout << "." << '\n';
                    }
                    counter++;
                }
            }
            break;
        }
        case DEBUG_QUIT_NOSAVE:
            if( query_yn(
                    _( "Quit without saving?  This may cause issues such as duplicated or missing items and vehicles!" ) ) ) {
                u.moves = 0;
                g->uquit = QUIT_NOSAVED;
            }
            break;
        case DEBUG_LUA_CONSOLE: {
            cata::show_lua_console();
            break;
        }
        case DEBUG_TEST_WEATHER: {
            get_weather().get_cur_weather_gen().test_weather( g->get_seed() );
        }
        break;

        case DEBUG_SAVE_SCREENSHOT:
            g->queue_screenshot = true;
            break;

        case DEBUG_BUG_REPORT: {
            constexpr const char *const bug_report_url =
                "https://github.com/cataclysmbn/Cataclysm-BN/issues/new"
                "?labels=bug"
                "&template=bug_report.yml"
                "&versions-and-configuration=";

            const std::string report = game_info::game_report();
            const std::string url = bug_report_url + encode_url( report );

            open_url( url );
            DebugLog( DL::Info, DC::Main ) << " GAME REPORT:\n" << report;
            popup( _( "Opened a link to Bug Report on github." ) );
            break;
        }
        case DEBUG_GAME_REPORT: {
            // generate a game report, useful for bug reporting.
            const std::string report = game_info::game_report();
            // write to log
            DebugLog( DL::Info, DC::Main ) << " GAME REPORT:\n" << report;
            std::string popup_msg = _( "Report written to debug.log" );
#if defined(TILES)
            // copy to clipboard
            int clipboard_result = SDL_SetClipboardText( report.c_str() );
            printErrorIf( clipboard_result != 0, "Error while copying the game report to the clipboard." );
            if( clipboard_result == 0 ) {
                popup_msg += _( " and to the clipboard." );
            }
#endif
            popup( popup_msg );
        }
        break;

        case DEBUG_JOIN_DISCORD: {
            open_url( "https://discord.gg/XW7XhXuZ89" );
            popup( _( "Opened a link to join Bright Nights Discord." ) );
            break;
        }

        case DEBUG_VEHICLE_BATTERY_CHARGE: {
            optional_vpart_position v_part_pos = g->m.veh_at( u.pos() );
            if( !v_part_pos ) {
                add_msg( m_bad, _( "There's no vehicle there." ) );
                break;
            }

            int amount = 0;
            string_input_popup popup;
            popup
            .title( _( "By how much?  (in kJ, negative to discharge)" ) )
            .width( 30 )
            .edit( amount );
            if( !popup.canceled() ) {
                vehicle &veh = v_part_pos->vehicle();
                if( amount >= 0 ) {
                    veh.charge_battery( amount, false );
                } else {
                    veh.discharge_battery( -amount, false );
                }
            }
            break;
        }
        case DEBUG_VEHICLE_EXPORT_JSON: {
            const optional_vpart_position v_part_pos = g->m.veh_at( u.pos() );
            if( !v_part_pos ) {
                add_msg( m_bad, _( "There's no vehicle there." ) );
                break;
            }
            const vehicle &veh = v_part_pos->vehicle();
            std::stringstream ss;
            JsonOut json( ss, true );
            json_export::vehicle( json, veh );

            // write to log
            DebugLog( DL::Info, DC::Main ) << " JSON TEMPLATE EXPORT:\n" << ss.str();
            std::string popup_msg = _( "JSON template written to debug.log" );
#if defined(TILES)
            // copy to clipboard
            const int clipboard_result = SDL_SetClipboardText( ss.str().c_str() );
            printErrorIf( clipboard_result != 0, "Error while exporting JSON to the clipboard." );
            if( clipboard_result == 0 ) {
                popup_msg += _( " and to the clipboard." );
            }
#endif
            popup( popup_msg );
            break;
        }

        case DEBUG_TEST_MAP_EXTRA_DISTRIBUTION:
            MapExtras::debug_spawn_test();
            break;
        case DEBUG_RESET_IGNORED_MESSAGES:
            debug_reset_ignored_messages();
            break;
        case DEBUG_RELOAD_TILES: {
            std::ostringstream ss;
            g->reload_tileset( [&ss]( const std::string & str ) {
                ss << str << '\n';
            } );
            add_msg( ss.str() );
            break;
        }
        case DEBUG_DUMP_TILES: {
#if defined(TILES) && defined(DYNAMIC_ATLAS)
            tilecontext->current_tileset()->texture_atlas()->readback_load();
            tilecontext->current_tileset()->texture_atlas()->readback_dump( PATH_INFO::config_dir() );
            tilecontext->current_tileset()->texture_atlas()->readback_clear();
#endif
            break;
        }
        case DEBUG_DISPLAY_TILESET_NO_VFX: {
            g->display_toggle_overlay( ACTION_DISPLAY_TILES_NO_VFX );
            break;
        }

    }
    m.invalidate_map_cache( g->get_levz() );
}

} // namespace debug_menu
