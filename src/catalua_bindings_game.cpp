#include "catalua_bindings.h"
#include "catalua_bindings_utils.h"
#include "catalua_impl.h"
#include "catalua_luna.h"
#include "catalua_luna_doc.h"

#include "avatar.h"
#include "distribution_grid.h"
#include "game.h"
#include "map.h"
#include "messages.h"
#include "npc.h"
#include "monster.h"

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

    luna::set_fx( lib, "create_item", []( const itype_id & itype, int count ) -> std::unique_ptr<item> { return std::make_unique<item>( itype, calendar::turn, count ); } );

    luna::set_fx( lib, "get_creature_at",
                  []( const tripoint & p, sol::optional<bool> allow_hallucination ) -> Creature * { return g->critter_at<Creature>( p, allow_hallucination.value_or( false ) ); } );
    luna::set_fx( lib, "get_monster_at",
                  []( const tripoint & p, sol::optional<bool> allow_hallucination ) -> monster * { return g->critter_at<monster>( p, allow_hallucination.value_or( false ) ); } );
    luna::set_fx( lib, "place_monster_at", []( const mtype_id & id, const tripoint & p ) { return g->place_critter_at( id, p ); } );
    luna::set_fx( lib, "place_monster_around", []( const mtype_id & id, const tripoint & p,
    const int radius ) { return g->place_critter_around( id, p, radius ); } );
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

    luna::finalize_lib( lib );
}
