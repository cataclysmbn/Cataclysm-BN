#include "game_observation.h"

#include <sstream>

#include "avatar.h"
#include "calendar.h"
#include "character.h"
#include "game.h"
#include "game_session.h"
#include "item.h"
#include "item_contents.h"
#include "units.h"
#include "map/map.h"
#include "map_iterator.h"
#include "overmap/overmapbuffer.h"
#include "overmap/overmapbuffer_registry.h"
#include "json.h"
#include "player_activity.h"

namespace game_observation
{
namespace
{

auto write_item( JsonOut &json, const item &thing, const bool include_contents = true ) -> void
{
    json.start_object();
    json.member( "type_id", thing.typeId().str() );
    json.member( "name", thing.tname_passive() );
    json.member( "count", thing.count() );
    json.member( "invlet", thing.invlet == 0 ? std::string{} : std::string( 1, thing.invlet ) );
    if( include_contents && !thing.contents.empty() ) {
        json.member( "contents" );
        json.start_array();
        for( const auto *contained : thing.contents.all_items_top() ) {
            if( contained != nullptr ) {
                write_item( json, *contained );
            }
        }
        json.end_array();
    }
    json.end_object();
}

auto write_avatar( JsonOut &json, const avatar &you ) -> void
{
    const auto pos = you.bub_pos();
    json.member( "name", you.name );
    json.member( "position" );
    json.start_object();
    json.member( "x", pos.x() );
    json.member( "y", pos.y() );
    json.member( "z", pos.z() );
    json.end_object();
    const auto absolute = you.abs_pos();
    json.member( "absolute_position" );
    json.start_object();
    json.member( "x", absolute.x() );
    json.member( "y", absolute.y() );
    json.member( "z", absolute.z() );
    json.end_object();
    json.member( "hp" );
    json.start_object();
    json.member( "current", you.get_hp() );
    json.member( "max", you.get_hp_max() );
    json.member( "percentage", you.hp_percentage() );
    json.end_object();
    json.member( "bodyparts" );
    json.start_array();
    for( const auto &bodypart : you.get_all_body_parts() ) {
        json.start_object();
        json.member( "id", bodypart.id().str() );
        json.member( "hp", you.get_part_hp_cur( bodypart ) );
        json.member( "hp_max", you.get_part_hp_max( bodypart ) );
        json.end_object();
    }
    json.end_array();
    json.member( "activity" );
    json.start_object();
    const auto activity_active = you.activity && !you.activity->complete();
    json.member( "active", activity_active );
    if( activity_active ) {
        json.member( "id", you.activity->id().str() );
    }
    json.end_object();
    json.member( "needs" );
    json.start_object();
    json.member( "stored_kcal", you.get_stored_kcal() );
    json.member( "max_kcal", you.max_stored_kcal() );
    json.member( "thirst", you.get_thirst() );
    json.member( "fatigue", you.get_fatigue() );
    json.member( "sleep_deprivation", you.get_sleep_deprivation() );
    json.member( "moves", you.get_moves() );
    json.member( "stamina", you.get_stamina() );
    json.member( "stamina_max", you.get_stamina_max() );
    json.member( "power_kj", units::to_kilojoule( you.get_power_level() ) );
    json.member( "power_max_kj", units::to_kilojoule( you.get_max_power_level() ) );
    json.end_object();
    json.member( "inventory" );
    json.start_array();
    for( const auto *stack : you.inv_const_slice() ) {
        if( stack != nullptr ) {
            for( const auto *thing : *stack ) {
                if( thing != nullptr ) {
                    write_item( json, *thing );
                }
            }
        }
    }
    json.end_array();
    json.member( "worn" );
    json.start_array();
    for( const auto *thing : you.worn ) {
        if( thing ) { write_item( json, *thing ); }
    }
    json.end_array();
    json.member( "wielded" );
    json.start_array();
    for( const auto *thing : you.wielded_items() ) {
        if( thing != nullptr ) {
            write_item( json, *thing );
        }
    }
    json.end_array();
}

auto write_visible_map( JsonOut &json, const game &current ) -> void
{
    const auto center = current.u.bub_pos();
    json.member( "visible_map" );
    json.start_array();
    for( const auto &pos : current.m.points_in_radius( center, 12 ) ) {
        if( !current.m.inbounds( pos ) || !current.u.sees( pos ) ) {
            continue;
        }
        json.start_object();
        json.member( "position" );
        json.start_object();
        json.member( "x", pos.x() );
        json.member( "y", pos.y() );
        json.member( "z", pos.z() );
        json.end_object();
        json.member( "terrain", current.m.ter( pos ).id().str() );
        json.member( "furniture", current.m.furn( pos ).id().str() );
        json.member( "traversable", current.m.passable( pos ) );
        const auto *creature = current.critter_at<Creature>( pos, true );
        if( creature != nullptr && current.u.sees( *creature ) ) {
            json.member( "creature" );
            json.start_object();
            json.member( "name", creature->get_name() );
            json.member( "player", creature->is_player() );
            json.end_object();
        }
        if( current.m.sees_some_items( pos, current.u ) ) {
            json.member( "items" );
            json.start_array();
            for( const auto *thing : current.m.i_at( pos ) ) {
                if( thing != nullptr ) {
                    write_item( json, *thing, false );
                }
            }
            json.end_array();
        }
        json.end_object();
    }
    json.end_array();
}

auto write_known_overmap( JsonOut &json, const game &current ) -> void
{
    const auto omt = project_to<coords::omt>( current.u.abs_pos() );
    auto &buffer = get_overmapbuffer( current.get_current_dimension_id() );
    json.member( "known_overmap" );
    json.start_array();
    const auto min = point{ -8, -8 };
    const auto max = point{ 8, 8 };
    for( const auto &offset : point_range<point>( min, max ) ) {
        const auto pos = tripoint_abs_omt{ omt.xy() + offset, omt.z() };
        if( !buffer.seen_loaded( pos ) ) {
            continue;
        }
        json.start_object();
        json.member( "x", pos.x() );
        json.member( "y", pos.y() );
        json.member( "z", pos.z() );
        json.member( "terrain", buffer.ter_loaded( pos ).id().str() );
        json.end_object();
    }
    json.end_array();
}

} // namespace

auto capture( const ::game *current ) -> snapshot
{
    auto output = std::ostringstream{};
    auto json = JsonOut( output );
    json.start_object();
    json.member( "session" );
    json.start_object();
    json.member( "running", game_session::running() );
    json.member( "turn", to_turn<int>( calendar::turn ) );
    json.member( "time", to_string( calendar::turn ) );
    json.end_object();
    const auto ready = game_session::running() && current != nullptr &&
                       current->get_active_world() != nullptr;
    json.member( "game_ready", ready );
    json.member( "coverage" );
    json.start_object();
    json.member( "visible_map_radius", 12 );
    json.member( "map_coordinate_space", "reality_bubble_map_squares" );
    json.member( "overmap_coordinate_space", "absolute_overmap_tiles" );
    json.member( "known_overmap_radius", 8 );
    json.member( "visibility_rule", "terrain and objects are included only when currently visible" );
    json.end_object();
    if( ready ) {
        json.member( "dimension", current->get_current_dimension_id().str() );
        json.member( "avatar" );
        json.start_object();
        write_avatar( json, current->u );
        json.end_object();
        write_visible_map( json, *current );
        write_known_overmap( json, *current );
    }
    json.end_object();
    return { .json = output.str(), .text = "Structured BN state: avatar, visible map, and known overmap." };
}

auto capture() -> snapshot
{
    return capture( g.get() );
}

} // namespace game_observation
