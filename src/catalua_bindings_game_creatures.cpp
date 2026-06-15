#include "catalua_bindings_game_internal.h"
#include "catalua_bindings_utils.h"
#include "catalua_luna_doc.h"

#include "avatar.h"
#include "creature_tracker.h"
#include "game.h"
#include "monster.h"
#include "npc.h"

namespace cata::detail
{

auto reg_game_api_creature_queries( luna::userlib &lib ) -> void
{
    DOC( "Returns all active creatures (monsters, NPCs, and the player) as a Lua array." );
    luna::set_fx( lib, "get_all_creatures", []( sol::this_state s ) -> sol::table {
        sol::state_view lua( s );
        auto out = lua.create_table();
        auto npc_rng = g->all_npcs();
        auto mon_rng = g->all_monsters();
        auto idx = 1;
        out[idx++] = static_cast<Creature *>( &g->u );
        if( npc_rng.items )
        {
            for( const auto &wp : *npc_rng.items ) {
                const auto sp = wp.lock();
                if( sp && !sp->is_dead() ) {
                    out[idx++] = static_cast<Creature *>( sp.get() );
                }
            }
        }
        if( mon_rng.items )
        {
            for( const auto &wp : *mon_rng.items ) {
                const auto sp = wp.lock();
                if( sp && !sp->is_dead() ) {
                    out[idx++] = static_cast<Creature *>( sp.get() );
                }
            }
        }
        return out;
    } );

    DOC( "Returns all active NPCs as a Lua array." );
    luna::set_fx( lib, "get_all_npcs", []( sol::this_state s ) -> sol::table {
        sol::state_view lua( s );
        auto out = lua.create_table();
        auto rng = g->all_npcs();
        auto idx = 1;
        if( rng.items )
        {
            for( const auto &wp : *rng.items ) {
                const auto sp = wp.lock();
                if( sp && !sp->is_dead() ) {
                    out[idx++] = sp.get();
                }
            }
        }
        return out;
    } );

    DOC( "Returns all active monsters as a Lua array." );
    luna::set_fx( lib, "get_all_monsters", []( sol::this_state s ) -> sol::table {
        sol::state_view lua( s );
        auto out = lua.create_table();
        auto rng = g->all_monsters();
        auto idx = 1;
        if( rng.items )
        {
            for( const auto &wp : *rng.items ) {
                const auto sp = wp.lock();
                if( sp && !sp->is_dead() ) {
                    out[idx++] = sp.get();
                }
            }
        }
        return out;
    } );

    DOC( "Returns active NPCs near an absolute overmap terrain tile as a Lua array.  " );
    DOC( "Takes a table with keys: `center`, `radius`, and optional `ignore_z`." );
    luna::set_fx( lib, "get_npcs_near_omt", []( sol::this_state s, sol::table params ) -> sol::table {
        sol::state_view lua( s );
        auto out = lua.create_table();
        const auto p = params["center"].get<tripoint_abs_omt>();
        const auto radius = params["radius"].get_or( 0 );
        const auto all_z = params["ignore_z"].get_or( false );
        auto idx = 1;
        auto npcs = g->all_npcs();
        if( npcs.items )
        {
            for( const auto &wp : *npcs.items ) {
                const auto sp = wp.lock();
                if( !sp || sp->is_dead() || sp->marked_for_death ) {
                    continue;
                }
                const auto pos = sp->abs_omt_pos();
                if( ( all_z || pos.z() == p.z() ) && square_dist( pos.xy(), p.xy() ) <= radius ) {
                    out[idx++] = sp.get();
                }
            }
        }
        return out;
    } );

    DOC( "Returns active monsters near an absolute overmap terrain tile as a Lua array.  " );
    DOC( "Takes a table with keys: `center`, `radius`, and optional `ignore_z`." );
    luna::set_fx( lib, "get_monsters_near_omt", []( sol::this_state s,
    sol::table params ) -> sol::table {
        sol::state_view lua( s );
        auto out = lua.create_table();
        const auto p = params["center"].get<tripoint_abs_omt>();
        const auto radius = params["radius"].get_or( 0 );
        const auto all_z = params["ignore_z"].get_or( false );
        auto idx = 1;
        for( const auto &sp : g->critter_tracker->get_monsters_list() )
        {
            if( !sp || sp->is_dead() ) {
                continue;
            }
            const auto pos = project_to<coords::omt>( sp->abs_pos() );
            if( ( all_z || pos.z() == p.z() ) && square_dist( pos.xy(), p.xy() ) <= radius ) {
                out[idx++] = sp.get();
            }
        }
        return out;
    } );

    DOC( "Returns NPCs in simulated (fully loaded, AI-eligible) submaps as a Lua array." );
    luna::set_fx( lib, "get_simulated_npcs", []( sol::this_state s ) -> sol::table {
        sol::state_view lua( s );
        auto out = lua.create_table();
        auto rng = g->all_npcs();
        auto idx = 1;
        if( rng.items )
        {
            for( const auto &wp : *rng.items ) {
                const auto sp = wp.lock();
                if( sp && !sp->is_dead() && sp->is_simulated() ) {
                    out[idx++] = sp.get();
                }
            }
        }
        return out;
    } );

    DOC( "Get the player's pet monsters" );
    luna::set_fx( lib, "get_player_pets", []( sol::this_state s ) -> sol::table {
        sol::state_view lua( s );
        auto out = lua.create_table();
        auto rng = g->all_monsters();
        auto idx = 1;
        if( rng.items )
        {
            for( const auto &wp : *rng.items ) {
                const auto sp = wp.lock();
                if( sp && !sp->is_dead() && sp->is_pet() ) {
                    out[idx++] = sp.get();
                }
            }
        }
        return out;
    } );
}

} // namespace cata::detail
