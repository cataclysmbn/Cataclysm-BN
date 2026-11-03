#include "player.h"

#include "avatar.h"
#include "enums.h"
#include "flag.h"
#include "game.h"
#include "map/map.h"
#include "messages.h"
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
    // Bad legacy API: player movement should be expressed in absolute coordinates.
    setpos( bub_to_abs( p ) );
}

auto player::setpos( const tripoint_abs_ms &p ) -> void
{
    const auto old_position = abs_pos();
    if( p == old_position ) {
        return;
    }

    position = p;
    if( g == nullptr || this != &g->u ) {
        return;
    }

    g->m.invalidate_visibility_caches();
    g->mon_info_cache_dirty = true;

    const auto old_submap = project_to<coords::sm>( old_position );
    const auto new_submap = project_to<coords::sm>( p );
    if( old_submap.xy() != new_submap.xy() ) {
        g->update_map( *this );
    }
    if( old_position.z() != p.z() ) {
        g->vertical_shift( old_position.z(), p.z() );
    }
}

auto player::setpos_no_map_update( const tripoint_abs_ms &p ) -> void
{
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
