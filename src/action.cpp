#include "action.h"

#include <algorithm>
#include <climits>
#include <istream>
#include <iterator>
#include <memory>
#include <optional>
#include <utility>

#include "avatar.h"
#include "cata_utility.h"
#include "catacharset.h"
#include "character.h"
#include "crafting.h"
#include "creature.h"
#include "cursesdef.h"
#include "debug.h"
#include "flag.h"
#include "game.h"
#include "iexamine.h"
#include "input.h"
#include "item.h"
#include "map.h"
#include "map_iterator.h"
#include "mapdata.h"
#include "messages.h"
#include "options.h"
#include "output.h"
#include "path_info.h"
#include "point.h"
#include "popup.h"
#include "ret_val.h"
#include "string_utils.h"
#include "translations.h"
#include "trap.h"
#include "type_id.h"
#include "ui.h"
#include "ui_manager.h"
#include "veh_type.h"
#include "vehicle.h"
#include "vehicle_part.h"
#include "vpart_position.h"

static const quality_id qual_BUTCHER( "BUTCHER" );
static const quality_id qual_CUT_FINE( "CUT_FINE" );

static const std::string flag_CONSOLE( "CONSOLE" );
static const std::string flag_GOES_DOWN( "GOES_DOWN" );
static const std::string flag_GOES_UP( "GOES_UP" );
static const std::string flag_SEALED( "SEALED" );
static const std::string flag_SWIMMABLE( "SWIMMABLE" );

static const trait_flag_str_id trait_flag_TAIL_FIN( "TAIL_FIN" );

class inventory;

std::vector<char> keys_bound_to( action_id act, const bool restrict_to_printable )
{
    input_context ctxt = get_default_mode_input_context();
    return ctxt.keys_bound_to( action_ident( act ), restrict_to_printable );
}

action_id action_from_key( char ch )
{
    input_context ctxt = get_default_mode_input_context();
    const input_event event( ch, input_event_t::keyboard );
    const std::string &action = ctxt.input_to_action( event );
    return look_up_action( action );
}

std::string action_ident( action_id act )
{
    switch( act ) {
        case ACTION_PAUSE:
            return "pause";
        case ACTION_TIMEOUT:
            return "TIMEOUT";
        case ACTION_MOVE_FORTH:
            return "UP";
        case ACTION_MOVE_FORTH_RIGHT:
            return "RIGHTUP";
        case ACTION_MOVE_RIGHT:
            return "RIGHT";
        case ACTION_MOVE_BACK_RIGHT:
            return "RIGHTDOWN";
        case ACTION_MOVE_BACK:
            return "DOWN";
        case ACTION_MOVE_BACK_LEFT:
            return "LEFTDOWN";
        case ACTION_MOVE_LEFT:
            return "LEFT";
        case ACTION_MOVE_FORTH_LEFT:
            return "LEFTUP";
        case ACTION_MOVE_DOWN:
            return "LEVEL_DOWN";
        case ACTION_MOVE_UP:
            return "LEVEL_UP";
        case ACTION_TOGGLE_MAP_MEMORY:
            return "toggle_map_memory";
        case ACTION_CENTER:
            return "center";
        case ACTION_SHIFT_N:
            return "shift_n";
        case ACTION_SHIFT_NE:
            return "shift_ne";
        case ACTION_SHIFT_E:
            return "shift_e";
        case ACTION_SHIFT_SE:
            return "shift_se";
        case ACTION_SHIFT_S:
            return "shift_s";
        case ACTION_SHIFT_SW:
            return "shift_sw";
        case ACTION_SHIFT_W:
            return "shift_w";
        case ACTION_SHIFT_NW:
            return "shift_nw";
        case ACTION_CYCLE_MOVE:
            return "cycle_move";
        case ACTION_RESET_MOVE:
            return "reset_move";
        case ACTION_TOGGLE_RUN:
            return "toggle_run";
        case ACTION_TOGGLE_CROUCH:
            return "toggle_crouch";
        case ACTION_OPEN_MOVEMENT:
            return "open_movement";
        case ACTION_OPEN:
            return "open";
        case ACTION_CLOSE:
            return "close";
        case ACTION_SMASH:
            return "smash";
        case ACTION_EXAMINE:
            return "examine";
        case ACTION_ADVANCEDINV:
            return "advinv";
        case ACTION_PICKUP:
            return "pickup";
        case ACTION_PICKUP_FEET:
            return "pickup_feet";
        case ACTION_GRAB:
            return "grab";
        case ACTION_HAUL:
            return "haul";
        case ACTION_BUTCHER:
            return "butcher";
        case ACTION_CHAT:
            return "chat";
        case ACTION_LOOK:
            return "look";
        case ACTION_PEEK:
            return "peek";
        case ACTION_LIST_ITEMS:
            return "listitems";
        case ACTION_ZONES:
            return "zones";
        case ACTION_LOOT:
            return "loot";
        case ACTION_INVENTORY:
            return "inventory";
        case ACTION_COMPARE:
            return "compare";
        case ACTION_ORGANIZE:
            return "organize";
        case ACTION_USE:
            return "apply";
        case ACTION_USE_WIELDED:
            return "apply_wielded";
        case ACTION_WEAR:
            return "wear";
        case ACTION_TAKE_OFF:
            return "take_off";
        case ACTION_EAT:
            return "eat";
        case ACTION_OPEN_CONSUME:
            return "open_consume";
        case ACTION_READ:
            return "read";
        case ACTION_WIELD:
            return "wield";
        case ACTION_PICK_STYLE:
            return "pick_style";
        case ACTION_RELOAD_ITEM:
            return "reload_item";
        case ACTION_RELOAD_WEAPON:
            return "reload_weapon";
        case ACTION_RELOAD_WIELDED:
            return "reload_wielded";
        case ACTION_UNLOAD:
            return "unload";
        case ACTION_MEND:
            return "mend";
        case ACTION_THROW:
            return "throw";
        case ACTION_FIRE:
            return "fire";
        case ACTION_FIRE_BURST:
            return "fire_burst";
        case ACTION_CAST_SPELL:
            return "cast_spell";
        case ACTION_SELECT_FIRE_MODE:
            return "select_fire_mode";
        case ACTION_SELECT_DEFAULT_AMMO:
            return "select_default_ammo";
        case ACTION_DROP:
            return "drop";
        case ACTION_DIR_DROP:
            return "drop_adj";
        case ACTION_BIONICS:
            return "bionics";
        case ACTION_MUTATIONS:
            return "mutations";
        case ACTION_SORT_ARMOR:
            return "sort_armor";
        case ACTION_WAIT:
            return "wait";
        case ACTION_CRAFT:
            return "craft";
        case ACTION_RECRAFT:
            return "recraft";
        case ACTION_LONGCRAFT:
            return "long_craft";
        case ACTION_CONSTRUCT:
            return "construct";
        case ACTION_DISASSEMBLE:
            return "disassemble";
        case ACTION_SALVAGE:
            return "salvage";
        case ACTION_SLEEP:
            return "sleep";
        case ACTION_CONTROL_VEHICLE:
            return "control_vehicle";
        case ACTION_TOGGLE_AUTO_TRAVEL_MODE:
            return "auto_travel_mode";
        case ACTION_TOGGLE_SAFEMODE:
            return "safemode";
        case ACTION_TOGGLE_AUTOSAFE:
            return "autosafe";
        case ACTION_TOGGLE_THIEF_MODE:
            return "toggle_thief_mode";
        case ACTION_IGNORE_ENEMY:
            return "ignore_enemy";
        case ACTION_WHITELIST_ENEMY:
            return "whitelist_enemy";
        case ACTION_SAVE:
            return "save";
        case ACTION_QUICKSAVE:
            return "quicksave";
        case ACTION_QUICKLOAD:
            return "quickload";
        case ACTION_SUICIDE:
            return "SUICIDE";
        case ACTION_PL_INFO:
            return "player_data";
        case ACTION_MAP:
            return "map";
        case ACTION_SKY:
            return "sky";
        case ACTION_MISSIONS:
            return "missions";
        case ACTION_FACTIONS:
            return "factions";
        case ACTION_SCORES:
            return "scores";
        case ACTION_MORALE:
            return "morale";
        case ACTION_MESSAGES:
            return "messages";
        case ACTION_OPEN_WIKI:
            return "open_wiki";
        case ACTION_OPEN_HHG:
            return "open_hhg";
        case ACTION_HELP:
            return "help";
        case ACTION_DEBUG:
            return "debug";
        case ACTION_LUA_CONSOLE:
            return "lua_console";
        case ACTION_LUA_RELOAD:
            return "lua_reload";
        case ACTION_DISPLAY_SCENT:
            return "debug_scent";
        case ACTION_DISPLAY_SCENT_TYPE:
            return "debug_scent_type";
        case ACTION_DISPLAY_TEMPERATURE:
            return "debug_temp";
        case ACTION_DISPLAY_VEHICLE_AI:
            return "debug_vehicle_ai";
        case ACTION_DISPLAY_VISIBILITY:
            return "debug_visibility";
        case ACTION_DISPLAY_TRANSPARENCY:
            return "debug_transparency";
        case ACTION_DISPLAY_LIGHTING:
            return "debug_lighting";
        case ACTION_DISPLAY_RADIATION:
            return "debug_radiation";
        case ACTION_DISPLAY_SUBMAP_GRID:
            return "debug_submap_grid";
        case ACTION_TOGGLE_ZONE_OVERLAY:
            return "toggle_zone_overlay";
        case ACTION_TOGGLE_HOUR_TIMER:
            return "debug_hour_timer";
        case ACTION_TOGGLE_DEBUG_MODE:
            return "debug_mode";
        case ACTION_ZOOM_OUT:
            return "zoom_out";
        case ACTION_ZOOM_IN:
            return "zoom_in";
        case ACTION_TOGGLE_FULLSCREEN:
            return "toggle_fullscreen";
        case ACTION_TOGGLE_PIXEL_MINIMAP:
            return "toggle_pixel_minimap";
        case ACTION_TOGGLE_PANEL_ADM:
            return "toggle_panel_adm";
        case ACTION_PANEL_MGMT:
            return "panel_mgmt";
        case ACTION_RELOAD_TILESET:
            return "reload_tileset";
        case ACTION_TOGGLE_AUTO_FEATURES:
            return "toggle_auto_features";
        case ACTION_TOGGLE_AUTO_PULP_BUTCHER:
            return "toggle_auto_pulp_butcher";
        case ACTION_TOGGLE_AUTO_MINING:
            return "toggle_auto_mining";
        case ACTION_TOGGLE_AUTO_FORAGING:
            return "toggle_auto_foraging";
        case ACTION_TOGGLE_AUTO_PICKUP:
            return "toggle_auto_pickup";
        case ACTION_ACTIONMENU:
            return "action_menu";
        case ACTION_ITEMACTION:
            return "item_action_menu";
        case ACTION_SELECT:
            return "SELECT";
        case ACTION_SEC_SELECT:
            return "SEC_SELECT";
        case ACTION_AUTOATTACK:
            return "autoattack";
        case ACTION_MAIN_MENU:
            return "main_menu";
        case ACTION_DIARY:
            return "diary";
        case ACTION_KEYBINDINGS:
            return "HELP_KEYBINDINGS";
        case ACTION_OPTIONS:
            return "open_options";
        case ACTION_AUTOPICKUP:
            return "open_autopickup";
        case ACTION_AUTONOTES:
            return "open_autonotes";
        case ACTION_SAFEMODE:
            return "open_safemode";
        case ACTION_COLOR:
            return "open_color";
        case ACTION_WORLD_MODS:
            return "open_world_mods";
        case ACTION_DISTRACTION_MANAGER:
            return "open_distraction_manager";
        case ACTION_TOGGLE_CHARACTER_PREVIEW_CLOTHES:
            return "TOGGLE_CHARACTER_PREVIEW_CLOTHES";
        case ACTION_NULL:
            return "null";
        case ACTION_SWAP_TO_NPC:
            return "SWAPTONPC";
        default:
            return "unknown";
    }
}

bool can_action_change_worldstate( const action_id act )
{
    switch( act ) {
        // Shift view
        case ACTION_TOGGLE_MAP_MEMORY:
        case ACTION_CENTER:
        case ACTION_SHIFT_N:
        case ACTION_SHIFT_NE:
        case ACTION_SHIFT_E:
        case ACTION_SHIFT_SE:
        case ACTION_SHIFT_S:
        case ACTION_SHIFT_SW:
        case ACTION_SHIFT_W:
        case ACTION_SHIFT_NW:
        // Environment Interaction
        case ACTION_LOOK:
        case ACTION_LIST_ITEMS:
        case ACTION_ZONES:
        // Long-term / special actions
        case ACTION_SAVE:
        case ACTION_QUICKSAVE:
        case ACTION_QUICKLOAD:
        case ACTION_SUICIDE:
        // Info Screens
        case ACTION_PL_INFO:
        case ACTION_MAP:
        case ACTION_SKY:
        case ACTION_MISSIONS:
        case ACTION_SCORES:
        case ACTION_FACTIONS:
        case ACTION_MORALE:
        case ACTION_MESSAGES:
        case ACTION_OPEN_WIKI:
        case ACTION_HELP:
        case ACTION_MAIN_MENU:
        case ACTION_KEYBINDINGS:
        case ACTION_OPTIONS:
        case ACTION_AUTOPICKUP:
        case ACTION_AUTONOTES:
        case ACTION_SAFEMODE:
        case ACTION_COLOR:
        case ACTION_WORLD_MODS:
        case ACTION_DISTRACTION_MANAGER:
        case ACTION_TOGGLE_CHARACTER_PREVIEW_CLOTHES:
        // Debug Functions
        case ACTION_TOGGLE_FULLSCREEN:
        case ACTION_DEBUG:
        case ACTION_LUA_CONSOLE:
        case ACTION_LUA_RELOAD:
        case ACTION_DISPLAY_SCENT:
        case ACTION_DISPLAY_SCENT_TYPE:
        case ACTION_DISPLAY_TEMPERATURE:
        case ACTION_DISPLAY_VEHICLE_AI:
        case ACTION_DISPLAY_VISIBILITY:
        case ACTION_DISPLAY_LIGHTING:
        case ACTION_DISPLAY_RADIATION:
        case ACTION_DISPLAY_TRANSPARENCY:
        case ACTION_DISPLAY_SUBMAP_GRID:
        case ACTION_TOGGLE_ZONE_OVERLAY:
        case ACTION_ZOOM_OUT:
        case ACTION_ZOOM_IN:
        case ACTION_TOGGLE_PIXEL_MINIMAP:
        case ACTION_TOGGLE_PANEL_ADM:
        case ACTION_PANEL_MGMT:
        case ACTION_RELOAD_TILESET:
        case ACTION_TIMEOUT:
        case ACTION_TOGGLE_AUTO_FEATURES:
        case ACTION_TOGGLE_AUTO_PULP_BUTCHER:
        case ACTION_TOGGLE_AUTO_MINING:
        case ACTION_TOGGLE_AUTO_FORAGING:
            return false;
        default:
            return true;
    }
}

action_id look_up_action( const std::string &ident )
{
    // Temporarily for the interface with the input manager!
    if( ident == "move_nw" ) {
        return ACTION_MOVE_FORTH_LEFT;
    } else if( ident == "move_sw" ) {
        return ACTION_MOVE_BACK_LEFT;
    } else if( ident == "move_ne" ) {
        return ACTION_MOVE_FORTH_RIGHT;
    } else if( ident == "move_se" ) {
        return ACTION_MOVE_BACK_RIGHT;
    } else if( ident == "move_n" ) {
        return ACTION_MOVE_FORTH;
    } else if( ident == "move_s" ) {
        return ACTION_MOVE_BACK;
    } else if( ident == "move_w" ) {
        return ACTION_MOVE_LEFT;
    } else if( ident == "move_e" ) {
        return ACTION_MOVE_RIGHT;
    } else if( ident == "move_down" ) {
        return ACTION_MOVE_DOWN;
    } else if( ident == "move_up" ) {
        return ACTION_MOVE_UP;
    }
    // ^^ Temporarily for the interface with the input manager!
    for( int i = 0; i < NUM_ACTIONS; i++ ) {
        if( action_ident( static_cast<action_id>( i ) ) == ident ) {
            return static_cast<action_id>( i );
        }
    }
    return ACTION_NULL;
}

// (Press X (or Y)|Try) to Z
std::string press_x( action_id act )
{
    input_context ctxt = get_default_mode_input_context();
    return ctxt.press_x( action_ident( act ), _( "Press " ), "", _( "Try" ) );
}
std::string press_x( action_id act, const std::string &key_bound, const std::string &key_unbound )
{
    input_context ctxt = get_default_mode_input_context();
    return ctxt.press_x( action_ident( act ), key_bound, "", key_unbound );
}
std::string press_x( action_id act, const std::string &key_bound_pre,
                     const std::string &key_bound_suf,
                     const std::string &key_unbound )
{
    input_context ctxt = get_default_mode_input_context();
    return ctxt.press_x( action_ident( act ), key_bound_pre, key_bound_suf, key_unbound );
}
std::optional<std::string> press_x_if_bound( action_id act )
{
    input_context ctxt = get_default_mode_input_context();
    std::string description = action_ident( act );
    if( ctxt.keys_bound_to( description ).empty() ) {
        return std::nullopt;
    }
    return press_x( act );
}

action_id get_movement_action_from_delta( const tripoint &d, const iso_rotate rot )
{
    if( d.z == -1 ) {
        return ACTION_MOVE_DOWN;
    } else if( d.z == 1 ) {
        return ACTION_MOVE_UP;
    }

    const bool iso_mode = rot == iso_rotate::yes && use_tiles && tile_iso;
    if( d.xy() == point_north ) {
        return iso_mode ? ACTION_MOVE_FORTH_LEFT : ACTION_MOVE_FORTH;
    } else if( d.xy() == point_north_east ) {
        return iso_mode ? ACTION_MOVE_FORTH : ACTION_MOVE_FORTH_RIGHT;
    } else if( d.xy() == point_east ) {
        return iso_mode ? ACTION_MOVE_FORTH_RIGHT : ACTION_MOVE_RIGHT;
    } else if( d.xy() == point_south_east ) {
        return iso_mode ? ACTION_MOVE_RIGHT : ACTION_MOVE_BACK_RIGHT;
    } else if( d.xy() == point_south ) {
        return iso_mode ? ACTION_MOVE_BACK_RIGHT : ACTION_MOVE_BACK;
    } else if( d.xy() == point_south_west ) {
        return iso_mode ? ACTION_MOVE_BACK : ACTION_MOVE_BACK_LEFT;
    } else if( d.xy() == point_west ) {
        return iso_mode ? ACTION_MOVE_BACK_LEFT : ACTION_MOVE_LEFT;
    } else {
        return iso_mode ? ACTION_MOVE_LEFT : ACTION_MOVE_FORTH_LEFT;
    }
}

point get_delta_from_movement_action( const action_id act, const iso_rotate rot )
{
    const bool iso_mode = rot == iso_rotate::yes && use_tiles && tile_iso;
    switch( act ) {
        case ACTION_MOVE_FORTH:
            return iso_mode ? point_north_east : point_north;
        case ACTION_MOVE_FORTH_RIGHT:
            return iso_mode ? point_east : point_north_east;
        case ACTION_MOVE_RIGHT:
            return iso_mode ? point_south_east : point_east;
        case ACTION_MOVE_BACK_RIGHT:
            return iso_mode ? point_south : point_south_east;
        case ACTION_MOVE_BACK:
            return iso_mode ? point_south_west : point_south;
        case ACTION_MOVE_BACK_LEFT:
            return iso_mode ? point_west : point_south_west;
        case ACTION_MOVE_LEFT:
            return iso_mode ? point_north_west : point_west;
        case ACTION_MOVE_FORTH_LEFT:
            return iso_mode ? point_north : point_north_west;
        default:
            return point_zero;
    }
}

int hotkey_for_action( action_id action, const bool restrict_to_printable )
{
    auto is_valid_key = []( char key ) {
        return key != '?';
    };
    std::vector<char> keys = keys_bound_to( action, restrict_to_printable );
    auto valid = std::ranges::find_if( keys, is_valid_key );
    return valid == keys.end() ? -1 : *valid;
}

bool can_butcher_at( const tripoint &p )
{
    avatar &you = get_avatar();
    // TODO: unify this with game::butcher
    const int factor = you.max_quality( qual_BUTCHER );
    const int factorD = you.max_quality( qual_CUT_FINE );
    map_stack items = get_map().i_at( p );
    bool has_item = false;
    bool has_corpse = false;

    const inventory &crafting_inv = you.crafting_inventory();
    for( item *&items_it : items ) {
        if( items_it->is_corpse() ) {
            if( factor != INT_MIN  || factorD != INT_MIN ) {
                has_corpse = true;
            }
        } else if( crafting::can_disassemble( you, *items_it, crafting_inv ).success() ) {
            has_item = true;
        }
    }
    return has_corpse || has_item;
}

bool can_move_vertical_at( const tripoint &p, int movez )
{
    map &here = get_map();
    // TODO: unify this with game::move_vertical
    if( here.has_flag( flag_SWIMMABLE, p ) && here.has_flag( TFLAG_DEEP_WATER, p ) ) {
        if( movez == -1 ) {
            return !g->u.is_underwater() && !g->u.worn_with_flag( flag_FLOTATION );
        } else {
            return g->u.swim_speed() < 500 || ( g->u.is_wearing( itype_id( "swim_fins" ) ) ||
                                                g->u.has_trait_flag( trait_flag_TAIL_FIN ) );
        }
    }

    if( movez == -1 ) {
        return here.has_flag( flag_GOES_DOWN, p );
    } else {
        return here.has_flag( flag_GOES_UP, p );
    }
}

bool can_examine_at( const tripoint &p )
{
    map &here = get_map();
    Character &u = get_player_character();
    if( here.veh_at( p ) ) {
        return true;
    }
    if( here.has_flag( flag_CONSOLE, p ) ) {
        return true;
    }
    if( here.has_items( p ) ) {
        return true;
    }
    const furn_t &xfurn_t = here.furn( p ).obj();
    const ter_t &xter_t = here.ter( p ).obj();

    if( here.has_furn( p ) && xfurn_t.examine != &iexamine::none ) {
        return true;
    } else if( xter_t.examine != &iexamine::none ) {
        return true;
    }

    Creature *c = g->critter_at( p );
    if( c != nullptr && p != u.pos() ) {
        return true;
    }

    return here.can_see_trap_at( p, u );
}

static bool can_pickup_at( const tripoint &p )
{
    bool veh_has_items = false;
    map &here = get_map();
    const optional_vpart_position vp = here.veh_at( p );
    if( vp ) {
        const int cargo_part = vp->vehicle().part_with_feature( vp->part_index(), "CARGO", false );
        veh_has_items = cargo_part >= 0 && !vp->vehicle().get_items( cargo_part ).empty();
    }
    return ( here.has_items( p ) && !here.has_flag( flag_SEALED, p ) ) || veh_has_items;
}

bool can_interact_at( action_id action, const tripoint &p )
{
    map &here = get_map();
    switch( action ) {
        case ACTION_OPEN:
            return here.can_open_door( &get_avatar(), p, !here.is_outside( g->u.pos() ) );
        case ACTION_CLOSE: {
            const optional_vpart_position vp = here.veh_at( p );
            return ( vp &&
                     vp->vehicle().next_part_to_close( vp->part_index(),
                             veh_pointer_or_null( here.veh_at( g->u.pos() ) ) != &vp->vehicle() ) >= 0 ) ||
                   here.close_door( p, !here.is_outside( g->u.pos() ), true );
        }
        case ACTION_BUTCHER:
            return can_butcher_at( p );
        case ACTION_MOVE_UP:
            return can_move_vertical_at( p, 1 );
        case ACTION_MOVE_DOWN:
            return can_move_vertical_at( p, -1 );
        case ACTION_EXAMINE:
            return can_examine_at( p );
        case ACTION_PICKUP:
        case ACTION_PICKUP_FEET:
            return can_pickup_at( p );
        default:
            return false;
    }
}

namespace
{
auto make_register_actions( std::vector<uilist_entry> &entries, const input_context &ctxt )
{
    return [&]( std::vector<action_id> &&names ) -> void {
        const auto fn = [&]( action_id name ) -> uilist_entry {
            return { name, true, hotkey_for_action( name ), ctxt.get_action_name( action_ident( name ) ) };
        };
        std::ranges::transform( names, std::back_inserter( entries ), fn );
    };
}

auto make_register_categories( std::vector<uilist_entry> &entries,
                               std::map<int, std::string> &categories_by_int,
                               int &last_category )
{
    return [&]( std::vector<std::string> &&names ) -> void {
        const auto fn = [&]( const std::string & name ) -> uilist_entry {
            categories_by_int[last_category] = name;
            return { last_category++, true, -1, name + "…" };
        };
        std::ranges::transform( names, std::back_inserter( entries ),  fn );
    };
}

} // namespace

action_id handle_action_menu()
{
    const input_context ctxt = get_default_mode_input_context();

    // Calculate weightings for the various actions to give the player suggestions
    // Weight >= 200: Special action only available right now
    std::map<action_id, int> action_weightings;

    // Check if we're in a potential combat situation, if so, sort a few actions to the top.
    if( !g->u.get_hostile_creatures( 60 ).empty() ) {
        // Only prioritize movement options if we're not driving.
        if( !g->u.controlling_vehicle ) {
            action_weightings[ACTION_CYCLE_MOVE] = 400;
        }
        // Only prioritize fire weapon options if we're wielding a ranged weapon.
        if( g->u.primary_weapon().is_gun() || g->u.primary_weapon().has_flag( flag_REACH_ATTACK ) ) {
            action_weightings[ACTION_FIRE] = 350;
        }
    }

    // If we're already running, make it simple to toggle running to off.
    if( g->u.movement_mode_is( CMM_RUN ) ) {
        action_weightings[ACTION_TOGGLE_RUN] = 300;
    }
    // If we're already crouching, make it simple to toggle crouching to off.
    if( g->u.movement_mode_is( CMM_CROUCH ) ) {
        action_weightings[ACTION_TOGGLE_CROUCH] = 300;
    }

    map &here = get_map();
    // Check if we're on a vehicle, if so, vehicle controls should be top.
    if( here.veh_at( g->u.pos() ) ) {
        // Make it 300 to prioritize it before examining the vehicle.
        action_weightings[ACTION_CONTROL_VEHICLE] = 300;
    }

    // Check if we can perform one of our actions on nearby terrain. If so,
    // display that action at the top of the list.
    for( const tripoint &pos : here.points_in_radius( g->u.pos(), 1 ) ) {
        if( pos != g->u.pos() ) {
            // Check for actions that work on nearby tiles, skipping tiles blocked by vehicles
            if( here.obstructed_by_vehicle_rotation( g->u.pos(), pos ) ) {
                continue;
            }

            if( can_interact_at( ACTION_OPEN, pos ) ) {
                action_weightings[ACTION_OPEN] = 200;
            }
            if( can_interact_at( ACTION_CLOSE, pos ) ) {
                action_weightings[ACTION_CLOSE] = 200;
            }
            if( can_interact_at( ACTION_EXAMINE, pos ) ) {
                action_weightings[ACTION_EXAMINE] = 200;
            }
        } else {
            // Check for actions that work on own tile only
            if( can_interact_at( ACTION_BUTCHER, pos ) ) {
                action_weightings[ACTION_BUTCHER] = 200;
            }
            if( can_interact_at( ACTION_MOVE_UP, pos ) ) {
                action_weightings[ACTION_MOVE_UP] = 200;
            }
            if( can_interact_at( ACTION_MOVE_DOWN, pos ) ) {
                action_weightings[ACTION_MOVE_DOWN] = 200;
            }
        }
    }

    // sort the map by its weightings
    std::vector<std::pair<action_id, int> > sorted_pairs;
    std::ranges::copy( action_weightings,
                       std::back_inserter<std::vector<std::pair<action_id, int> > >( sorted_pairs ) );
    std::ranges::reverse( sorted_pairs );

    // Default category is called "back"
    std::string category = "back";

    while( true ) {
        std::vector<uilist_entry> entries;
        uilist_entry *entry;
        std::map<int, std::string> categories_by_int;
        int last_category = NUM_ACTIONS + 1;

        const auto register_actions = make_register_actions( entries, ctxt );
        const auto register_action_if_hotkey_assigned = [&]( action_id action ) {
            if( hotkey_for_action( action ) > -1 ) {
                register_actions( { action } );
            }
        };
        const auto register_categories =
            make_register_categories( entries, categories_by_int, last_category );

        if( category == "back" ) {
            for( const auto &[ action, weight ] : sorted_pairs ) {
                if( weight >= 200 ) {
                    register_actions( { action } );
                }
            }

            register_categories( {
                _( "Look" ), _( "Interact" ), _( "Inventory" ),
                _( "Combat" ), _( "Craft" ), _( "Info" ), _( "Misc" )
            } );

            register_action_if_hotkey_assigned( ACTION_QUICKSAVE );
            register_actions( { ACTION_SAVE } );
            register_action_if_hotkey_assigned( ACTION_QUICKLOAD );
            register_action_if_hotkey_assigned( ACTION_SUICIDE );
            register_actions( { ACTION_OPEN_WIKI, ACTION_OPEN_HHG, ACTION_HELP } );
            if( ( entry = &entries.back() ) ) {
                // help _is_a menu.
                entry->txt += "…";
            }
            if( hotkey_for_action( ACTION_DEBUG ) > -1 ) {
                // register with global key
                register_categories( { _( "Debug" ) } );
                if( ( entry = &entries.back() ) ) {
                    entry->hotkey = hotkey_for_action( ACTION_DEBUG );
                }
            }
        } else if( category == _( "Look" ) ) {
            register_actions( { ACTION_LOOK, ACTION_PEEK, ACTION_LIST_ITEMS, ACTION_ZONES, ACTION_MAP, ACTION_SKY } );
        } else if( category == _( "Inventory" ) ) {
            register_actions( {ACTION_INVENTORY, ACTION_ADVANCEDINV, ACTION_SORT_ARMOR, ACTION_DIR_DROP } );

            // Everything below here can be accessed through
            // the inventory screen, so it's sorted to the
            // end of the list.
            register_actions( {
                ACTION_DROP, ACTION_COMPARE, ACTION_ORGANIZE, ACTION_USE,
                ACTION_WEAR, ACTION_TAKE_OFF, ACTION_EAT, ACTION_OPEN_CONSUME,
                ACTION_READ, ACTION_WIELD, ACTION_UNLOAD
            } );
        } else if( category == _( "Debug" ) ) {
            register_actions( { ACTION_DEBUG } );
            if( ( entry = &entries.back() ) ) {
                // debug _is_a menu.
                entry->txt += "…";
            }
#if !defined(TILES)
            register_actions( { ACTION_TOGGLE_FULLSCREEN } );
#endif
#if defined(TILES)
            register_actions( { ACTION_TOGGLE_PIXEL_MINIMAP, ACTION_RELOAD_TILESET  } );
#endif // TILES
            register_actions( {
                ACTION_TOGGLE_PANEL_ADM, ACTION_DISPLAY_SCENT, ACTION_DISPLAY_SCENT_TYPE,
                ACTION_DISPLAY_TEMPERATURE, ACTION_DISPLAY_VEHICLE_AI, ACTION_DISPLAY_VISIBILITY,
                ACTION_DISPLAY_LIGHTING, ACTION_DISPLAY_TRANSPARENCY, ACTION_DISPLAY_RADIATION,
                ACTION_DISPLAY_SUBMAP_GRID, ACTION_TOGGLE_ZONE_OVERLAY, ACTION_TOGGLE_DEBUG_MODE
            } );
        } else if( category == _( "Interact" ) ) {
            register_actions( {
                ACTION_EXAMINE, ACTION_SMASH, ACTION_MOVE_DOWN, ACTION_MOVE_UP,
                ACTION_OPEN, ACTION_CLOSE, ACTION_CHAT, ACTION_PICKUP,
                ACTION_PICKUP_FEET, ACTION_GRAB, ACTION_HAUL, ACTION_BUTCHER, ACTION_LOOT,
            } );
        } else if( category == _( "Combat" ) ) {
            register_actions( {
                ACTION_CYCLE_MOVE, ACTION_RESET_MOVE, ACTION_TOGGLE_RUN, ACTION_TOGGLE_CROUCH,
                ACTION_OPEN_MOVEMENT, ACTION_FIRE, ACTION_RELOAD_ITEM, ACTION_RELOAD_WEAPON,
                ACTION_RELOAD_WIELDED, ACTION_CAST_SPELL, ACTION_SELECT_FIRE_MODE,
                ACTION_SELECT_DEFAULT_AMMO, ACTION_THROW, ACTION_FIRE_BURST, ACTION_PICK_STYLE,
                ACTION_TOGGLE_AUTO_TRAVEL_MODE, ACTION_TOGGLE_SAFEMODE, ACTION_TOGGLE_AUTOSAFE,
                ACTION_IGNORE_ENEMY, ACTION_TOGGLE_AUTO_FEATURES, ACTION_TOGGLE_AUTO_PULP_BUTCHER,
                ACTION_TOGGLE_AUTO_MINING, ACTION_TOGGLE_AUTO_FORAGING
            } );
        } else if( category == _( "Craft" ) ) {
            register_actions( {
                ACTION_CRAFT, ACTION_RECRAFT, ACTION_LONGCRAFT,
                ACTION_CONSTRUCT, ACTION_DISASSEMBLE, ACTION_SALVAGE
            } );
        } else if( category == _( "Info" ) ) {
            register_actions( {
                ACTION_PL_INFO, ACTION_MISSIONS, ACTION_SCORES,
                ACTION_FACTIONS, ACTION_MORALE, ACTION_MESSAGES, ACTION_DIARY
            } );
        } else if( category == _( "Misc" ) ) {
            register_actions( {
                ACTION_WAIT, ACTION_SLEEP, ACTION_BIONICS, ACTION_MUTATIONS,
                ACTION_CONTROL_VEHICLE, ACTION_ITEMACTION, ACTION_TOGGLE_THIEF_MODE
            } );
#if defined(TILES)
            if( use_tiles ) {
                register_actions( { ACTION_ZOOM_OUT, ACTION_ZOOM_IN } );
            }
#endif
        }

        if( category != "back" ) {
            std::string msg = _( "Back" );
            entries.emplace_back( 2 * NUM_ACTIONS, true,
                                  hotkey_for_action( ACTION_ACTIONMENU ), msg + "…" );
        }

        std::string title = _( "Actions" );
        if( category != "back" ) {
            std::string catgname = category;
            capitalize_letter( catgname, 0 );
            title += ": " + catgname;
        }

        uilist smenu;
        smenu.settext( title );
        smenu.entries = entries;
        smenu.query();
        const int selection = smenu.ret;

        if( selection < 0 || selection == NUM_ACTIONS ) {
            return ACTION_NULL;
        } else if( selection == 2 * NUM_ACTIONS ) {
            if( category != "back" ) {
                category = "back";
            } else {
                return ACTION_NULL;
            }
        } else if( selection > NUM_ACTIONS ) {
            category = categories_by_int[selection];
        } else {
            return static_cast<action_id>( selection );
        }
    }
}

action_id handle_main_menu()
{
    const input_context ctxt = get_default_mode_input_context();
    std::vector<uilist_entry> entries;

    const auto register_actions = make_register_actions( entries, ctxt );

    register_actions( {
        ACTION_OPEN_WIKI, ACTION_OPEN_HHG, ACTION_HELP, ACTION_KEYBINDINGS, ACTION_OPTIONS, ACTION_AUTOPICKUP, ACTION_AUTONOTES,
        ACTION_SAFEMODE, ACTION_DISTRACTION_MANAGER, ACTION_COLOR, ACTION_WORLD_MODS,
        ACTION_ACTIONMENU, ACTION_QUICKSAVE, ACTION_SAVE, ACTION_DEBUG, ACTION_LUA_CONSOLE,
        ACTION_LUA_RELOAD
    } );

    uilist smenu;
    smenu.settext( _( "MAIN MENU" ) );
    smenu.entries = entries;
    smenu.query();
    int selection = smenu.ret;

    if( selection < 0 || selection >= NUM_ACTIONS ) {
        return ACTION_NULL;
    } else {
        return static_cast<action_id>( selection );
    }
}

std::optional<tripoint> choose_direction( const std::string &message, const bool allow_vertical )
{
    input_context ctxt( "DEFAULTMODE" );
    ctxt.set_iso( true );
    ctxt.register_directions();
    ctxt.register_action( "pause" );
    ctxt.register_action( "QUIT" );
    ctxt.register_action( "HELP_KEYBINDINGS" );
    if( allow_vertical ) {
        ctxt.register_action( "LEVEL_UP" );
        ctxt.register_action( "LEVEL_DOWN" );
    }

    static_popup popup;
    //~ %s: "Close where?" "Pry where?" etc.
    popup.message( _( "%s (Direction button)" ), message ).on_top( true );

    std::string action;
    do {
        ui_manager::redraw();
        action = ctxt.handle_input();
        if( const std::optional<tripoint> vec = ctxt.get_direction( action ) ) {
            // Make player's sprite face left/right if interacting with something to the left or right
            if( vec->x > 0 ) {
                g->u.facing = FD_RIGHT;
            } else if( vec->x < 0 ) {
                g->u.facing = FD_LEFT;
            }
            return vec;
        } else if( action == "pause" ) {
            return tripoint_zero;
        } else if( action == "LEVEL_UP" ) {
            return tripoint_above;
        } else if( action == "LEVEL_DOWN" ) {
            return tripoint_below;
        }
    } while( action != "QUIT" );

    add_msg( _( "Never mind." ) );
    return std::nullopt;
}

std::optional<tripoint> choose_adjacent( const std::string &message, const bool allow_vertical )
{
    const std::optional<tripoint> dir = choose_direction( message, allow_vertical );

    if( !dir ) {
        return std::nullopt;
    }

    if( get_map().obstructed_by_vehicle_rotation( g->u.pos(), *dir + g->u.pos() ) ) {
        add_msg( _( "You can't reach through that vehicle's wall." ) );
        return std::nullopt;
    }

    return *dir + g->u.pos();
}

std::optional<tripoint> choose_adjacent_highlight( const std::string &message,
        const std::string &failure_message, const action_id action, bool allow_vertical )
{
    const std::function<bool( const tripoint & )> f = [&action]( const tripoint & p ) {
        return can_interact_at( action, p );
    };
    return choose_adjacent_highlight( message, failure_message, f, allow_vertical );
}

std::optional<tripoint> choose_adjacent_highlight(
    const std::string &message,
    const std::string &failure_message,
    const std::function < auto( const tripoint & ) -> bool > &allowed,
    const bool allow_vertical )
{
    std::vector<tripoint> valid;
    map &here = get_map();
    if( allowed ) {
        for( const tripoint &pos : here.points_in_radius( g->u.pos(), 1 ) ) {
            if( !here.obstructed_by_vehicle_rotation( g->u.pos(), pos ) && allowed( pos ) ) {
                valid.emplace_back( pos );
            }
        }
    }

    const bool auto_select = get_option<bool>( "AUTOSELECT_SINGLE_VALID_TARGET" );
    if( valid.empty() && auto_select ) {
        add_msg( failure_message );
        return std::nullopt;
    } else if( valid.size() == 1 && auto_select ) {
        return valid.back();
    }

    shared_ptr_fast<game::draw_callback_t> hilite_cb;
    if( !valid.empty() ) {
        hilite_cb = make_shared_fast<game::draw_callback_t>( [&]() {
            for( const tripoint &pos : valid ) {
                here.drawsq( g->w_terrain, pos, drawsq_params().highlight( true ) );
            }
        } );
        g->add_draw_callback( hilite_cb );
    }

    return choose_adjacent( message, allow_vertical );
}
