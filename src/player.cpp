#include "player.h"

#include <map>
#include <memory>
#include <string>

#include "enums.h"
#include "flag.h"
#include "game.h"
#include "map.h"
#include "messages.h"
#include "output.h"
#include "string_id.h"
#include "vitamin.h"

static auto update_map_after_player_setpos( player &who,
        const tripoint_abs_ms &old_pos ) -> void
{
    if( g == nullptr || !who.is_avatar() ) {
        return;
    }

    const auto new_pos = who.abs_pos();
    if( old_pos == new_pos ) {
        return;
    }

    const auto old_sm = project_to<coords::sm>( old_pos );
    const auto new_sm = project_to<coords::sm>( new_pos );

    if( old_sm.xy() != new_sm.xy() ) {
        g->update_map( who );
    }
    if( old_pos.z() != new_pos.z() ) {
        g->vertical_shift( old_pos.z(), new_pos.z() );
    }
}

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
        vitamin_levels[ v.first ] = 0;
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
    const auto old_pos = position;
    position = p;
    update_map_after_player_setpos( *this, old_pos );
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
