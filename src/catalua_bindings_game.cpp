#include "catalua_bindings.h"
#include "catalua_coord.h"
#include "catalua_bindings_utils.h"
#include "catalua_bindings_game_internal.h"
#include "catalua_impl.h"
#include "catalua_luna.h"
#include "catalua_luna_doc.h"

#include <algorithm>
#include <ranges>
#include <stdexcept>
#include <vector>

#include "avatar.h"
#include "creature_tracker.h"
#include "distribution_grid.h"
#include "game.h"
#include "iexamine.h"
#include "lightmap.h"
#include "map.h"
#include "catalua_log.h"
#include "messages.h"
#include "npc.h"
#include "monster.h"
#include "overmapbuffer.h"
#include "weather.h"
#include "line.h"
#include "lua_action_menu.h"

namespace
{

void add_msg_lua( game_message_type t, sol::variadic_args va )
{
    if( va.size() == 0 ) {
        // Nothing to print
        return;
    }

    std::string msg = cata::detail::fmt_lua_va( va );
    add_msg( t, msg );
}

auto place_lua_monster_around( const mtype_id &id, const tripoint_bub_ms &center,
                               const int radius ) -> monster * // *NOPAD*
{
    const auto placed = g->place_critter_around( id, center, radius );
    if( placed != nullptr ) {
        placed->try_upgrade( true );
    }
    return placed;
}



} // namespace

void cata::detail::reg_game_api( sol::state &lua )
{
    DOC( "Global game methods" );
    luna::userlib lib = luna::begin_lib( lua, "gapi" );

    luna::set_fx( lib, "get_avatar", &get_avatar );
    luna::set_fx( lib, "get_map", &get_map );
    luna::set_fx( lib, "bub_to_abs", []( const tripoint_bub_ms & p ) -> tripoint_abs_ms { return bub_to_abs( p ); } );
    luna::set_fx( lib, "bub_to_abs", []( const tripoint_bub_sm & p ) -> tripoint_abs_sm { return bub_to_abs( p ); } );
    luna::set_fx( lib, "abs_to_bub", []( const tripoint_abs_ms & p ) -> tripoint_bub_ms { return abs_to_bub( p ); } );
    luna::set_fx( lib, "abs_to_bub", []( const tripoint_abs_sm & p ) -> tripoint_bub_sm { return abs_to_bub( p ); } );
    luna::set_fx( lib, "get_distribution_grid_tracker", &get_distribution_grid_tracker );
    luna::set_fx( lib, "light_ambient_lit", []() -> float { return LIGHT_AMBIENT_LIT; } );
    luna::set_fx( lib, "add_msg", sol::overload(
    add_msg_lua, []( sol::variadic_args va ) { add_msg_lua( game_message_type::m_neutral, va ); }
                  ) );
    DOC( "Teleports player to absolute coordinate in overmap" );
    luna::set_fx( lib, "place_player_overmap_at", []( const tripoint_abs_omt & p ) -> void { g->place_player_overmap( p ); } );
    DOC( "Teleports player to local coordinates within active map" );
    luna::set_fx( lib, "place_player_local_at", []( const tripoint_bub_ms & p ) -> void { g->place_player( p ); } );
    luna::set_fx( lib, "current_turn", []() -> time_point { return calendar::turn; } );
    luna::set_fx( lib, "turn_zero", []() -> time_point { return calendar::turn_zero; } );
    luna::set_fx( lib, "before_time_starts", []() -> time_point { return calendar::before_time_starts; } );
    luna::set_fx( lib, "bodytemp_cold", []() -> int { return BODYTEMP_COLD; } );
    luna::set_fx( lib, "bodytemp_norm", []() -> int { return BODYTEMP_NORM; } );
    luna::set_fx( lib, "bodytemp_hot", []() -> int { return BODYTEMP_HOT; } );
    luna::set_fx( lib, "rng", sol::resolve<int( int, int )>( &rng ) );
    DOC( "Get recent player message log entries. Returns array of { time=string, text=string }." );
    luna::set_fx( lib, "get_messages", []( sol::this_state lua_this, const int count ) {
        sol::state_view lua( lua_this );
        auto out = lua.create_table();
        const auto clamped = std::max( 0, count );
        auto entries = Messages::recent_messages( static_cast<size_t>( clamped ) );
        auto indices = std::views::iota( size_t{ 0 }, entries.size() );
        std::ranges::for_each( indices, [&]( const size_t idx ) {
            const auto &entry = entries[idx];
            auto row = lua.create_table_with(
                           "time", entry.first,
                           "text", entry.second
                       );
            out[idx + 1] = row;
        } );
        return out;
    } );
    DOC( "Get recent Lua console log entries. Returns array of { level=string, text=string, from_user=bool }." );
    luna::set_fx( lib, "get_lua_log", []( sol::this_state lua_this, const int count ) {
        sol::state_view lua( lua_this );
        auto out = lua.create_table();
        const auto clamped = std::max( 0, count );
        const auto &entries = cata::get_lua_log_instance().get_entries();
        const auto take = std::min( static_cast<size_t>( clamped ), entries.size() );
        auto indices = std::views::iota( size_t{ 0 }, take );
        const auto level_name = []( const cata::LuaLogLevel level ) -> std::string {
            switch( level )
            {
                case cata::LuaLogLevel::Input:
                    return "input";
                case cata::LuaLogLevel::Info:
                    return "info";
                case cata::LuaLogLevel::Warn:
                    return "warn";
                case cata::LuaLogLevel::Error:
                    return "error";
                case cata::LuaLogLevel::DebugMsg:
                    return "debug";
            }
            return "unknown";
        };
        std::ranges::for_each( indices, [&]( const size_t idx ) {
            const auto &entry = entries[idx];
            auto row = lua.create_table_with(
                           "level", level_name( entry.level ),
                           "text", entry.text,
                           "from_user", entry.level == cata::LuaLogLevel::Input
                       );
            out[idx + 1] = row;
        } );
        return out;
    } );
    luna::set_fx( lib, "add_on_every_x_hook",
    []( sol::this_state lua_this, time_duration interval, sol::protected_function f ) {
        sol::state_view lua( lua_this );
        std::vector<on_every_x_hooks> &hooks = lua["game"]["cata_internal"]["on_every_x_hooks"];
        for( auto &entry : hooks ) {
            if( entry.interval == interval ) {
                entry.functions.push_back( f );
                return;
            }
        }
        std::vector<sol::protected_function> vec;
        vec.push_back( f );
        hooks.push_back( on_every_x_hooks{ interval, vec } );
    } );

    DOC( "Register a Lua-defined action menu entry in the in-game action menu." );
    luna::set_fx( lib, "register_action_menu_entry", []( sol::table opts ) -> void {
        auto id = opts.get_or( "id", std::string{} );
        auto name = opts.get_or( "name", std::string{} );
        auto category_id = opts.get_or( "category", std::string{ "misc" } );
        auto hotkey = opts.get<sol::optional<std::string>>( "hotkey" );
        auto hotkey_value = std::optional<std::string>{};
        if( hotkey )
        {
            hotkey_value = std::move( *hotkey );
        }
        auto fn = opts.get_or<sol::protected_function>( "fn", sol::lua_nil );
        cata::lua_action_menu::register_entry( {
            .id = std::move( id ),
            .name = std::move( name ),
            .category_id = std::move( category_id ),
            .hotkey = std::move( hotkey_value ),
            .fn = std::move( fn ),
        } );
    } );

    DOC( "Spawns a new item. Same as Item::spawn " );
    luna::set_fx( lib, "create_item", []( const itype_id & itype, int count ) -> detached_ptr<item> {
        return item::spawn( itype, calendar::turn, count );
    } );

    luna::set_fx( lib, "get_creature_at", sol::overload(
    []( const tripoint_bub_ms & p, sol::optional<bool> allow_hallucination ) -> Creature * {
        return g->critter_at<Creature>( p, allow_hallucination.value_or( false ) );
    },
    []( const tripoint & p, sol::optional<bool> allow_hallucination ) -> Creature * {
        return g->critter_at<Creature>( tripoint_bub_ms( p ), allow_hallucination.value_or( false ) );
    } ) );
    luna::set_fx( lib, "get_monster_at", sol::overload(
    []( const tripoint_bub_ms & p, sol::optional<bool> allow_hallucination ) -> monster * {
        return g->critter_at<monster>( p, allow_hallucination.value_or( false ) );
    },
    []( const tripoint & p, sol::optional<bool> allow_hallucination ) -> monster * {
        return g->critter_at<monster>( tripoint_bub_ms( p ), allow_hallucination.value_or( false ) );
    } ) );
    luna::set_fx( lib, "place_monster_at", sol::overload(
    []( const mtype_id & id, const tripoint_bub_ms & p ) { return place_lua_monster_around( id, p, 0 ); },
    []( const mtype_id & id, const tripoint & p ) { return place_lua_monster_around( id, tripoint_bub_ms( p ), 0 ); } ) );
    luna::set_fx( lib, "place_monster_around", sol::overload(
    []( const mtype_id & id, const tripoint_bub_ms & p, const int radius ) {
        return place_lua_monster_around( id, p, radius );
    },
    []( const mtype_id & id, const tripoint & p, const int radius ) {
        return place_lua_monster_around( id, tripoint_bub_ms( p ), radius );
    } ) );
    luna::set_fx( lib, "spawn_hallucination", sol::overload(
                      []( const tripoint_bub_ms & p ) -> bool { return g->spawn_hallucination( p ); },
                      []( const tripoint & p ) -> bool { return g->spawn_hallucination( tripoint_bub_ms( p ) ); } ) );
    luna::set_fx( lib, "get_character_at", sol::overload(
    []( const tripoint_bub_ms & p, sol::optional<bool> allow_hallucination ) -> Character * {
        return g->critter_at<Character>( p, allow_hallucination.value_or( false ) );
    },
    []( const tripoint & p, sol::optional<bool> allow_hallucination ) -> Character * {
        return g->critter_at<Character>( tripoint_bub_ms( p ), allow_hallucination.value_or( false ) );
    } ) );
    luna::set_fx( lib, "get_npc_at", sol::overload(
    []( const tripoint_bub_ms & p, sol::optional<bool> allow_hallucination ) -> npc * {
        return g->critter_at<npc>( p, allow_hallucination.value_or( false ) );
    },
    []( const tripoint & p, sol::optional<bool> allow_hallucination ) -> npc * {
        return g->critter_at<npc>( tripoint_bub_ms( p ), allow_hallucination.value_or( false ) );
    } ) );

    luna::set_fx( lib, "choose_adjacent",
                  []( const std::string & message,
    sol::optional<bool> allow_vertical ) -> std::optional<tripoint_bub_ms> {
        return choose_adjacent( message, allow_vertical.value_or( false ) );
    } );
    luna::set_fx( lib, "choose_adjacent_highlight", sol::overload(
                      []( const std::string & message, const std::string & failure_message, const action_id & actionId,
    sol::optional<bool> allow_vertical ) -> std::optional<tripoint_bub_ms> {
        return choose_adjacent_highlight( message, failure_message, actionId, allow_vertical.value_or( false ) );
    }, [](
        const std::string & message,
        const std::string & failure_message,
        const std::function < auto( const tripoint_bub_ms & ) -> bool > &allowed,
        sol::optional<bool> allow_vertical
    ) -> std::optional<tripoint_bub_ms> {
        return choose_adjacent_highlight( message, failure_message, allowed, allow_vertical.value_or( false ) );
    } ) );
    luna::set_fx( lib, "choose_adjacent_uilist", [](
                      const std::string & message,
                      const std::string & failure_message,
                      const sol::protected_function & allowed,
                      const sol::protected_function & name
    ) -> std::optional<tripoint_bub_ms> {
        return choose_adjacent_uilist( message, failure_message, allowed, name );
    } );
    luna::set_fx( lib, "choose_area", [](
                      const std::string & message,
                      const tripoint_bub_ms & start_pos,
                      const bool allow_vertical
    ) -> std::optional<std::pair<tripoint_bub_ms, tripoint_bub_ms>> {
        return choose_area( message, start_pos, allow_vertical );
    } );

    luna::set_fx( lib, "choose_direction", []( const std::string & message,
    sol::optional<bool> allow_vertical ) -> std::optional<tripoint_rel_ms> {
        return choose_direction( message, allow_vertical.value_or( false ) );
    } );
    luna::set_fx( lib, "look_around", []() {
        return g->look_around();
    } );

    luna::set_fx( lib, "play_variant_sound",
                  sol::overload(
                      sol::resolve<void( const std::string &, const std::string &, int )>( &sfx::play_variant_sound ),
                      sol::resolve<void( const std::string &, const std::string &, int,
                                         units::angle, double, double )>( &sfx::play_variant_sound )
                  ) );
    luna::set_fx( lib, "play_ambient_variant_sound", &sfx::play_ambient_variant_sound );

    luna::set_fx( lib, "add_npc_follower", []( npc & p ) { g->add_npc_follower( p.getID() ); } );
    luna::set_fx( lib, "remove_npc_follower", []( npc & p ) { g->remove_npc_follower( p.getID() ); } );

    reg_game_api_creature_queries( lib );
    reg_game_api_world_helpers( lib );

    luna::finalize_lib( lib );
}
