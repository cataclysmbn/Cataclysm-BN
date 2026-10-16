#include "engine_client_contract.h"

#include "action.h"
#include "avatar.h"
#include "client_input.h"
#include "creature.h"
#include "enums.h"
#include "game.h"
#include "input.h"
#include "line.h"
#include "map/map.h"
#include "monster.h"
#include "npc.h"
#include "npctalk.h"
#include "point.h"
#include "translations.h"
#include "travel/travel_destination.h"

#include <algorithm>
#include <ranges>

namespace engine_client
{
namespace
{
constexpr auto tile_schema = std::string_view { "tile_action" };

/// The native action of the map's input context, named and keyed as that context has it.
auto native_action( const input_context &context, const action_id act ) -> tile_action
{
    const auto id = action_ident( act );
    return { .id = id, .label = context.get_action_name( id ),
             .keys = key_names( inp_mngr.get_input_for_action( id, "DEFAULTMODE" ) ) };
}
} // namespace

auto tile_actions( const tripoint_bub_ms &at ) -> std::vector<tile_action>
{
    const auto &you = get_avatar();
    const auto &here = get_map();
    if( !here.inbounds( at ) ) { return {}; }
    const auto context = get_default_mode_input_context();
    const auto self = at == you.bub_pos();
    const auto near = at.z() == you.bub_pos().z() && square_dist( at.xy(), you.bub_pos().xy() ) <= 1 &&
                      !here.obstructed_by_vehicle_rotation( you.bub_pos(), at );
    const auto *const monster = g->critter_at<::monster>( at );
    const auto *const person = g->critter_at<npc>( at );
    const auto seen_monster = monster != nullptr && you.sees( *monster );
    // As the native mouse, steps and routes do not steer a vehicle the avatar drives.
    const auto walks = !you.controlling_vehicle;
    auto result = std::vector<tile_action> {};
    const auto native = [&]( const action_id act ) { result.push_back( native_action( context, act ) ); };
    // As moving into it attacks: a monster that is not friendly, a person that is hostile.
    const auto foe = ( seen_monster && monster->attitude_to( you ) != A_FRIENDLY ) ||
                     ( person != nullptr && you.sees( *person ) && person->is_enemy() );
    if( walks && foe && near && !self ) {
        result.push_back( { .id = "attack", .label = _( "Attack" ) } );
    }
    if( seen_monster && you.primary_weapon().is_gun() ) { native( ACTION_FIRE ); }
    if( person != nullptr && !self && can_chat_with( you, *person ) ) {
        result.push_back( { .id = "talk", .label = _( "Talk" ) } );
    }
    if( near && !self && can_interact_at( ACTION_OPEN, at ) ) { native( ACTION_OPEN ); }
    if( near && !self && can_interact_at( ACTION_CLOSE, at ) ) { native( ACTION_CLOSE ); }
    if( near && can_interact_at( ACTION_PICKUP, at ) ) { native( ACTION_PICKUP ); }
    if( near && can_interact_at( ACTION_EXAMINE, at ) ) { native( ACTION_EXAMINE ); }
    if( walks && !self && avatar_knows_travel_destination( you, at ) ) {
        result.push_back( { .id = "travel", .label = _( "Travel here" ) } );
    }
    native( ACTION_LOOK );
    return result;
}

auto tile_operation_event( const tile_operation &operation ) -> std::optional<input_event>
{
    const auto target = to_bubble( operation.target, current_bubble_frame() );
    if( !target ) { return std::nullopt; }
    auto result = input_event{};
    result.type = input_event_t::interaction;
    result.interaction = game_client::interaction_event{
        .operation = game_client::interaction_operation::set_target,
        .schema_id = std::string( tile_schema ),
        .target_id = operation.action_id,
        .position = target,
    };
    return result;
}

auto tile_operation_of( const input_event &event ) -> std::optional<tile_command>
{
    const auto &semantic = event.interaction;
    if( !semantic || semantic->schema_id != tile_schema || !semantic->position ) { return std::nullopt; }
    const auto &at = *semantic->position;
    return tile_command{ .action_id = semantic->target_id, .at = tripoint_bub_ms( at.x, at.y, at.z ) };
}
} // namespace engine_client
