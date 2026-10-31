#include "player.h"

#include "enums.h"
#include "avatar.h"
#include "flag.h"
#include "game.h"
#include "map/map.h"
#include "messages.h"
#include "monster.h"
#include "output.h"
#include "string_id.h"
#include "vitamin.h"

#include <map>
#include <memory>
#include <string>

player::player()
{
    str_cur = 8;
    str_max = 8;
    dex_cur = 8;
    dex_max = 8;
    int_cur = 8;
    int_max = 8;
    per_cur = 8;
    per_max = 8;
    dodges_left = 1;
    blocks_left = 1;
    set_power_level( 0_kJ );
    set_max_power_level( 0_kJ );
    scent = 500;
    male = true;
    remove_primary_weapon();

    moves = 100;
    oxygen = 0;
    in_vehicle = false;
    controlling_vehicle = false;
    hauling = false;
    focus_pool = 100;
    last_item = itype_id( "null" );
    sight_max = 9999;
    last_batch = 0;
    lastconsumed = itype_id( "null" );
    death_drops = true;

    set_value( "THIEF_MODE", "THIEF_ASK" );

    for( const auto &v : vitamin::all() ) {
        vitamin_levels[ v.id ] = 0;
    }

    if( g != nullptr && json_flag::is_ready() && get_anatomy().is_valid() ) {
        // TODO: Remove the set_body here
        set_body();
        recalc_sight_limits();
        reset_encumbrance();
    }
}

player::~player() = default;
player::player( player && )  noexcept = default;
player &player::operator=( player && )  noexcept = default;

auto player::setpos( const tripoint_bub_ms &p ) -> void
{
    setpos( map_local_to_abs( get_map(), p ) );
}

auto player::setpos( const tripoint_abs_ms &p ) -> void
{
    if( p == abs_pos() ) {
        return;
    }

    if( g != nullptr && this == &g->u ) {
        g->clear_avatar_grab();
        if( in_vehicle ) {
            g->m.unboard_vehicle( bub_pos() );
        }
        if( is_mounted() ) {
            dismount();
        }
        g->set_player_position( p );
    } else {
        set_internal_position( p );
    }
}

auto player::set_internal_position( const tripoint_abs_ms &p ) -> void
{
    // This is deliberately only the coordinate write.  Map realignment belongs to
    // game::set_player_position so lifecycle and specialized movement callers can choose it
    // explicitly without inheriting teleport or ordinary movement effects.
    position = p;
    if( g != nullptr && this == &g->u ) {
        g->m.invalidate_visibility_caches();
        g->mon_info_cache_dirty = true;
    }
}


//message related stuff
void player::add_msg_if_player( const std::string &msg ) const
{
    Messages::add_msg( msg );
}

void player::add_msg_player_or_npc( const std::string &player_msg,
                                    const std::string &/*npc_msg*/ ) const
{
    Messages::add_msg( player_msg );
}

void player::add_msg_if_player( const game_message_params &params,
                                const std::string &msg ) const
{
    Messages::add_msg( params, msg );
}

void player::add_msg_player_or_npc( const game_message_params &params,
                                    const std::string &player_msg,
                                    const std::string &/*npc_msg*/ ) const
{
    Messages::add_msg( params, player_msg );
}

void player::add_msg_player_or_say( const std::string &player_msg,
                                    const std::string &/*npc_speech*/ ) const
{
    Messages::add_msg( player_msg );
}

void player::add_msg_player_or_say( const game_message_params &params,
                                    const std::string &player_msg,
                                    const std::string &/*npc_speech*/ ) const
{
    Messages::add_msg( params, player_msg );
}

bool player::query_yn( const std::string &mes ) const
{
    return ::query_yn( mes );
}
