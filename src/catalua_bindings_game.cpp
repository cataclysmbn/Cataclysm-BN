#include "catalua_bindings.h"
#include "catalua_bindings_utils.h"
#include "catalua_impl.h"
#include "catalua_luna.h"
#include "catalua_luna_doc.h"

#include <ranges>

#include "avatar.h"
#include "distribution_grid.h"
#include "game.h"
#include "lightmap.h"
#include "map.h"
#include "catalua_log.h"
#include "messages.h"
#include "npc.h"
#include "monster.h"
#include "overmapbuffer.h"
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

} // namespace

void cata::detail::reg_game_api( sol::state &lua )
{
    DOC( "Global game methods" );
    luna::userlib lib = luna::begin_lib( lua, "gapi" );

    luna::set_fx( lib, "get_avatar", &get_avatar );
    luna::set_fx( lib, "get_map", &get_map );
    luna::set_fx( lib, "get_distribution_grid_tracker", &get_distribution_grid_tracker );
    luna::set_fx( lib, "light_ambient_lit", []() -> float { return LIGHT_AMBIENT_LIT; } );
    luna::set_fx( lib, "add_msg", sol::overload(
    add_msg_lua, []( sol::variadic_args va ) { add_msg_lua( game_message_type::m_neutral, va ); }
                  ) );
    DOC( "Teleports player to absolute coordinate in overmap" );
    luna::set_fx( lib, "place_player_overmap_at", []( const tripoint & p ) -> void { g->place_player_overmap( tripoint_abs_omt( p ) ); } );
    DOC( "Teleports player to local coordinates within active map" );
    luna::set_fx( lib, "place_player_local_at", []( const tripoint & p ) -> void { g->place_player( p ); } );
    luna::set_fx( lib, "current_turn", []() -> time_point { return calendar::turn; } );
    luna::set_fx( lib, "turn_zero", []() -> time_point { return calendar::turn_zero; } );
    luna::set_fx( lib, "before_time_starts", []() -> time_point { return calendar::before_time_starts; } );
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
            switch( level ) {
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
        auto callback = opts.get_or<sol::protected_function>( "callback", sol::lua_nil );
        cata::lua_action_menu::register_entry( {
            .id = std::move( id ),
            .name = std::move( name ),
            .category_id = std::move( category_id ),
            .hotkey = std::move( hotkey_value ),
            .callback = std::move( callback ),
        } );
    } );

    DOC( "Spawns a new item. Same as Item::spawn " );
    luna::set_fx( lib, "create_item", []( const itype_id & itype, int count ) -> detached_ptr<item> {
        return item::spawn( itype, calendar::turn, count );
    } );

    luna::set_fx( lib, "get_creature_at",
                  []( const tripoint & p, sol::optional<bool> allow_hallucination ) -> Creature * { return g->critter_at<Creature>( p, allow_hallucination.value_or( false ) ); } );
    luna::set_fx( lib, "get_monster_at",
                  []( const tripoint & p, sol::optional<bool> allow_hallucination ) -> monster * { return g->critter_at<monster>( p, allow_hallucination.value_or( false ) ); } );
    luna::set_fx( lib, "place_monster_at", []( const mtype_id & id, const tripoint & p ) { return g->place_critter_at( id, p ); } );
    luna::set_fx( lib, "place_monster_around", []( const mtype_id & id, const tripoint & p,
    const int radius ) { return g->place_critter_around( id, p, radius ); } );
    luna::set_fx( lib, "spawn_hallucination", []( const tripoint & p ) -> bool { return g->spawn_hallucination( p ); } );
    luna::set_fx( lib, "get_character_at",
                  []( const tripoint & p, sol::optional<bool> allow_hallucination ) -> Character * { return g->critter_at<Character>( p, allow_hallucination.value_or( false ) ); } );
    luna::set_fx( lib, "get_npc_at",
                  []( const tripoint & p, sol::optional<bool> allow_hallucination ) -> npc * { return g->critter_at<npc>( p, allow_hallucination.value_or( false ) ); } );

    luna::set_fx( lib, "choose_adjacent",
    []( const std::string & message, sol::optional<bool> allow_vertical ) -> sol::optional<tripoint> {
        std::optional<tripoint> stdOpt = choose_adjacent( message, allow_vertical.value_or( false ) );

        if( stdOpt.has_value() )
        {
            return sol::optional<tripoint>( *stdOpt );
        }
        return sol::optional<tripoint>();
    } );
    luna::set_fx( lib, "choose_direction", []( const std::string & message,
    sol::optional<bool> allow_vertical ) -> sol::optional<tripoint> {
        std::optional<tripoint> stdOpt = choose_direction( message, allow_vertical.value_or( false ) );

        if( stdOpt.has_value() )
        {
            return sol::optional<tripoint>( *stdOpt );
        }
        return sol::optional<tripoint>();
    } );
    luna::set_fx( lib, "look_around", []() {
        auto result = g->look_around();
        if( result.has_value() ) {
            return sol::optional<tripoint>( *result );
        }
        return sol::optional<tripoint>();
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

    DOC( "Get the global overmap buffer" );
    luna::set_fx( lib, "get_overmap_buffer", []() -> overmapbuffer & { return overmap_buffer; } );

    DOC( "Get direction from a tripoint delta" );
    luna::set_fx( lib, "direction_from", []( const tripoint & delta ) -> direction { return direction_from( delta ); } );

    DOC( "Get direction name from direction enum" );
    luna::set_fx( lib, "direction_name", []( direction dir ) -> std::string { return direction_name( dir ); } );

    DOC( "Get the six cardinal directions (N, S, E, W, Up, Down)" );
    luna::set_fx( lib, "six_cardinal_directions", []() -> std::vector<tripoint> {
        return std::vector<tripoint>{
            tripoint_north, tripoint_south, tripoint_east,
            tripoint_west, tripoint_above, tripoint_below
        };
    } );

    luna::finalize_lib( lib );
}
