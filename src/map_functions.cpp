#include "map_functions.h"

#include "character.h"
#include "game.h"
#include "map.h"
#include "mapbuffer.h"
#include "mapbuffer_registry.h"
#include "map_iterator.h"
#include "messages.h"
#include "monster.h"
#include "sounds.h"

static const mtype_id mon_mi_go_myrmidon( "mon_mi_go_myrmidon" );

namespace
{

auto remove_migo_nerve_cage_terrain( mapbuffer &buffer, const tripoint_abs_ms &p ) -> bool
{
    auto open = false;
    const auto tile_reader = buffer.make_abs_tile_reader();
    for( const auto &tmp : points_in_radius( p, 12 ) ) {
        const auto tile = tile_reader.get_tile( tmp );
        if( tile && tile->get_ter() == ter_id( "t_wall_resin_cage" ) ) {
            buffer.set_ter( tmp, ter_id( "t_floor_resin" ) );
            open = true;
        }
    }
    return open;
}

auto finish_migo_nerve_cage_removal( const tripoint_bub_ms &p, const bool spawn_damaged,
                                     const bool open ) -> void
{
    if( open ) {
        add_msg( m_good, _( "The nerve cluster collapses in on itself, and the nearby cages open!" ) );
    } else {
        add_msg( _( "The nerve cluster collapses in on itself, to no discernible effect." ) );
    }
    sound_event se;
    se.origin = p;
    se.volume = 120;
    se.category = sounds::sound_t::combat;
    se.description = _( "a loud alien shriek reverberating through the structure!" );
    se.id = "shout";
    se.variant = "scream_tortured";
    sounds::sound( se );
    monster *const spawn = g->place_critter_around( mon_mi_go_myrmidon, p, 1 );
    if( spawn_damaged ) {
        spawn->set_hp( spawn->get_hp_max() / 2 );
    }
    // Don't give the mi-go free shots against the player
    spawn->mod_moves( -300 );
    if( get_player_character().sees( p ) ) {
        add_msg( m_bad, _( "Something stirs and clambers out of the ruined mass of flesh and nerves!" ) );
    }
}

} // namespace

namespace map_funcs
{

auto climbing_cost( mapbuffer &buffer, const tripoint_abs_ms &from,
                    const tripoint_abs_ms &to ) -> std::optional<int>
{
    // TODO: All sorts of mutations, equipment weight etc. for characters
    const auto move_options = mapbuffer_valid_move_options {
        .flying = true,
    };
    if( !buffer.valid_move( from, to, move_options ) ) {
        return {};
    }

    const auto diff = buffer.climb_difficulty( from );
    if( !diff || *diff > 5 ) {
        return {};
    }
    return 50 + *diff * 100;
}

auto climbing_cost( const map &m, const tripoint_bub_ms &from,
                    const tripoint_bub_ms &to ) -> std::optional<int>
{
    return climbing_cost( MAPBUFFER_REGISTRY.get( m.get_bound_dimension() ),
                          map_local_to_abs( m, from ), map_local_to_abs( m, to ) );
}

auto migo_nerve_cage_removal( mapbuffer &buffer, const tripoint_abs_ms &p,
                              const bool spawn_damaged ) -> void
{
    finish_migo_nerve_cage_removal( abs_to_bub( p ), spawn_damaged,
                                    remove_migo_nerve_cage_terrain( buffer, p ) );
}

void migo_nerve_cage_removal( map &m, const tripoint_bub_ms &p, bool spawn_damaged )
{
    auto &buffer = MAPBUFFER_REGISTRY.get( m.get_bound_dimension() );
    finish_migo_nerve_cage_removal( p, spawn_damaged,
                                    remove_migo_nerve_cage_terrain( buffer, map_local_to_abs( m, p ) ) );
}

} // namespace map_funcs
