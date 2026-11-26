#include <algorithm>
#include <print>

#include "catalua_bindings_game_internal.h"
#include "catalua_bindings_utils.h"
#include "catalua_luna_doc.h"

#include "avatar.h"
#include "catalua/catalua_creature_filters.h"
#include "creature_tracker.h"
#include "game.h"
#include "monster.h"
#include "npc.h"

namespace cata::detail
{

auto reg_game_api_creature_queries( luna::userlib &lib ) -> void
{
    DOC( "Returns all active creatures as an array of Creature: the player first, then living NPCs, then living monsters." );
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

    DOC( "Returns all living active NPCs as an array of Npc." );
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

    DOC( "Returns all living active monsters as an array of Monster." );
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

    DOC( "Returns active monsters matching every given filter as an array of Monster.  " );
    DOC( "Takes a table where each key is a filter; unknown keys show a debug message and are ignored. Available filters:  " );
    DOC( "* `type_ids` (MonsterTypeId[]): Monster type is one of these.  " );
    DOC( "* `faction_ids` (MonsterFactionId[]): Monster faction is one of these.  " );
    DOC( "* `species_ids` (SpeciesTypeId[]): Monster has at least one of these species.  " );
    DOC( "* `sees` (Monster[]): At least one of these monsters can see it.  " );
    DOC( "* `hostile_to` (Monster[]): It is hostile to at least one of these monsters.  " );
    DOC( "* `within_range_of` (table): { range = number, monsters = Monster[] }. It is within `range` tiles of at least one of these monsters.  " );
    DOC( "* `limit` (integer): Stop searching once this many monsters have matched." );
    luna::set_fx( lib, "get_monsters_if", []( sol::this_state s, sol::table filters ) -> sol::table {
        sol::state_view lua( s );
        auto out = lua.create_table();
        const auto monsters = filter_monsters_from_lua( filters );
        if( !monsters.empty() )
        {
            for( std::size_t i = 0; i < monsters.size(); ++i ) {
                out[i + 1] = monsters[i];
            }
        }
        return out;
    } );

    DOC( "Returns active NPCs near an absolute overmap terrain tile as an array of Npc.  " );
    DOC( "Takes a table with keys:  " );
    DOC( "* `center` (TripointAbsOmt): Overmap terrain tile to search around.  " );
    DOC( "* `radius` (integer, optional): Search radius in overmap terrain tiles. Defaults to 0.  " );
    DOC( "* `ignore_z` (boolean, optional): If true, matches every z-level. Defaults to false." );
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

    DOC( "Returns active monsters near an absolute overmap terrain tile as an array of Monster.  " );
    DOC( "Takes a table with keys:  " );
    DOC( "* `center` (TripointAbsOmt): Overmap terrain tile to search around.  " );
    DOC( "* `radius` (integer, optional): Search radius in overmap terrain tiles. Defaults to 0.  " );
    DOC( "* `ignore_z` (boolean, optional): If true, matches every z-level. Defaults to false." );
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

    DOC( "Returns living NPCs in simulated (fully loaded, AI-eligible) submaps as an array of Npc." );
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

    DOC( "Returns the player's living active pet monsters as an array of Monster." );
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
