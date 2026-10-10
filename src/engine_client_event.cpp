#include "engine_client_event.h"

#include <algorithm>
#include <limits>
#include <set>
#include <type_traits>
#include <utility>

namespace engine_client
{
namespace
{
static_assert( std::is_nothrow_move_assignable_v<state_value> );
static_assert( std::is_nothrow_move_assignable_v<snapshot> );

/// One event must fit a frame with room for its siblings in the same notification.
constexpr auto maximum_event_bytes = maximum_frame_bytes / 2;

auto inside( const bounds &area, const position &at ) -> bool
{
    return at.dim == area.min.dim && at.x >= area.min.x && at.x <= area.max.x &&
           at.y >= area.min.y && at.y <= area.max.y && at.z >= area.min.z && at.z <= area.max.z;
}
auto valid_event( const event_value &value ) -> bool
{
    const auto message = value.type == "message.logged";
    const auto transient = message || value.presentation.has_value();
    return value.sequence != 0 && ( value.revision != 0 || transient ) && !value.type.empty() &&
           message == value.message.has_value() && ( !transient || value.delta.empty() ) &&
           ( !value.presentation || ( !message && value.presentation->type == value.type ) ) &&
           ( !value.message || ( value.message->id != 0 && !value.message->text.empty() &&
                                 value.message->count != 0 ) ) &&
           ( !value.cause || ( *value.cause != 0 && *value.cause < value.sequence ) ) &&
           ( !value.command || ( !value.command->empty() && value.command->size() <= maximum_id_bytes ) );
}
/// Knowledge decides which facts a value may carry (design 3.3): remembered cells only their
/// memory, sensed cells and entities no appearance. A capture that violates it never publishes.
auto valid_cell( const cell &value ) -> bool
{
    const auto live = value.terrain || value.furniture || !value.fields.empty() ||
                      !value.traps.empty() ||
                      !value.items.empty() || value.vehicle || value.light || value.reviving;
    switch( value.known ) {
        case knowledge::remembered:
            return !live && value.memory;
        case knowledge::visible:
            return true;
        case knowledge::sensed:
            return !live && !value.memory;
    }
    return false;
}
auto valid_entity( const entity &value ) -> bool
{
    switch( value.known ) {
        case knowledge::visible:
            return !value.sense;
        case knowledge::sensed:
            return !value.appearance && !value.name && value.overlays.empty() && value.traits.empty() &&
                   !value.attitude &&
                   !value.aware &&
                   value.statuses.empty();
        case knowledge::remembered:
            return false;
    }
    return false;
}
auto valid_world( const world_state &world ) -> bool
{
    return std::ranges::all_of( world.cells, []( const auto & entry ) { return valid_cell( entry.second ); } )
    &&
    std::ranges::all_of( world.entities, []( const auto & entry ) { return valid_entity( entry.second ); } );
}
auto validate_state( const state_value &value ) -> std::expected<void, error>
{
    if( !valid_world( value.world ) ) { return std::unexpected( error::validation_failed ); }
    return validate_boundary( value.interaction );
}
/// The diff cannot say that a coverage, avatar or environment disappeared.
auto removes_value( const state_value &from, const state_value &to ) -> bool
{
    return ( from.world.coverage && !to.world.coverage ) || ( from.world.view && !to.world.view ) ||
           ( from.world.avatar && !to.world.avatar ) ||
           ( from.world.environment && !to.world.environment );
}
template<typename Key, typename Value, typename Remove>
auto erase_where( std::map<Key, Value> &values, const Remove &remove ) -> void
{
    for( auto it = values.begin(); it != values.end(); ) {
        if( remove( *it ) ) { it = values.erase( it ); }
        else { ++it; }
    }
}
} // namespace

auto changes::empty() const -> bool
{
    return !coverage && !view && cells.empty() && forgotten.empty() && entities.empty() &&
           gone.empty() &&
           !avatar && !environment && !route && !interaction;
}
auto same_state( const state_value &left, const state_value &right ) -> bool
{
    return left.world == right.world && same_boundary( left.interaction, right.interaction );
}

auto diff( const state_value &from, const state_value &to ) -> changes
{
    auto result = changes{};
    if( to.world.coverage && to.world.coverage != from.world.coverage ) {
        result.coverage = to.world.coverage;
    }
    if( to.world.view && to.world.view != from.world.view ) { result.view = to.world.view; }
    // Cells and entities outside a new coverage are dropped by the coverage itself.
    const auto survives = [&]( const position & at ) {
        return !result.coverage || inside( *result.coverage, at );
    };
    for( const auto &[at, value] : to.world.cells ) {
        const auto old = from.world.cells.find( at );
        if( old == from.world.cells.end() || old->second != value ) { result.cells.push_back( value ); }
    }
    for( const auto &[at, value] : from.world.cells ) {
        if( survives( at ) && !to.world.cells.contains( at ) ) { result.forgotten.push_back( at ); }
    }
    for( const auto &[id, value] : to.world.entities ) {
        const auto old = from.world.entities.find( id );
        if( old == from.world.entities.end() || old->second != value ) { result.entities.push_back( value ); }
    }
    for( const auto &[id, value] : from.world.entities ) {
        if( survives( value.at ) && !to.world.entities.contains( id ) ) { result.gone.push_back( {.id = id} ); }
    }
    if( to.world.avatar && to.world.avatar != from.world.avatar ) { result.avatar = to.world.avatar; }
    if( to.world.environment && to.world.environment != from.world.environment ) {
        result.environment = to.world.environment;
    }
    if( to.world.route != from.world.route ) { result.route = to.world.route; }
    if( !same_boundary( from.interaction, to.interaction ) ) { result.interaction = to.interaction; }
    return result;
}

auto apply( state_value &value, const changes &delta ) -> std::expected<void, error>
{
    auto next = value;
    auto &world = next.world;
    if( delta.coverage ) {
        world.coverage = delta.coverage;
        erase_where( world.cells, [&]( const auto & entry ) { return !inside( *delta.coverage, entry.first ); } );
        erase_where( world.entities, [&]( const auto & entry ) { return !inside( *delta.coverage, entry.second.at ); } );
    }
    if( delta.view ) { world.view = delta.view; }
    auto seen_cells = std::set<position> {};
    for( const auto &entry : delta.cells ) {
        if( !world.coverage || !inside( *world.coverage, entry.at ) ||
            !seen_cells.insert( entry.at ).second ) {
            return std::unexpected( error::resync_required );
        }
        world.cells[entry.at] = entry;
    }
    for( const auto &at : delta.forgotten ) {
        if( world.cells.erase( at ) == 0 ) { return std::unexpected( error::resync_required ); }
    }
    auto seen_entities = std::set<std::string> {};
    for( const auto &entry : delta.entities ) {
        if( !seen_entities.insert( entry.id ).second ) { return std::unexpected( error::resync_required ); }
        world.entities[entry.id] = entry;
    }
    for( const auto &entry : delta.gone ) {
        if( world.entities.erase( entry.id ) == 0 ) { return std::unexpected( error::resync_required ); }
    }
    if( delta.avatar ) { world.avatar = delta.avatar; }
    if( delta.environment ) { world.environment = delta.environment; }
    if( delta.route ) { world.route = *delta.route; }
    if( delta.interaction ) { next.interaction = *delta.interaction; }
    value = std::move( next );
    return {};
}

auto classify( const changes &delta ) -> std::string
{
    if( delta.coverage ) { return "coverage.moved"; }
    if( !delta.cells.empty() || !delta.forgotten.empty() || !delta.entities.empty() ||
        !delta.gone.empty() || delta.avatar || delta.route || delta.view ) { return "cells.seen"; }
    if( delta.environment ) { return "turn.passed"; }
    return "interaction.changed";
}

auto remember( std::vector<message_value> &log, const message_value &line ) -> void
{
    const auto old = std::ranges::find( log, line.id, &message_value::id );
    if( old != log.end() ) {
        *old = line;
        return;
    }
    log.push_back( line );
    if( log.size() > maximum_log_lines ) { log.erase( log.begin() ); }
}

public_event::public_event( event_value value ) : value_( std::move( value ) ) {}
auto public_event::value() const -> const event_value & { return value_; } // *NOPAD*

event_stream::event_stream( snapshot initial ) : current_( std::move( initial ) ) {}
auto event_stream::create( std::string epoch,
                           state_value initial ) -> std::expected<event_stream, error>
{
    if( epoch.empty() || epoch.size() > maximum_id_bytes ) { return std::unexpected( error::validation_failed ); }
    if( const auto valid = validate_state( initial ); !valid ) { return std::unexpected( valid.error() ); }
    return event_stream{snapshot{.at = {.epoch = std::move( epoch )}, .value = std::move( initial )}};
}
auto event_stream::seed_log( std::vector<message_value> lines ) -> void
{
    for( const auto &line : lines ) { remember( current_.log, line ); }
}
auto event_stream::current() const -> const snapshot & { return current_; } // *NOPAD*

auto event_stream::publish( publish_request request ) ->
std::expected<std::optional<public_event>, error>
{
    if( request.decision == disclosure::withheld ) { return std::nullopt; }
    if( const auto valid = validate_state( request.next ); !valid ) { return std::unexpected( valid.error() ); }
    if( removes_value( current_.value, request.next ) ) { return std::unexpected( error::resync_required ); }
    if( request.cause && ( *request.cause == 0 || *request.cause > current_.at.sequence ) ) {
        return std::unexpected( error::validation_failed );
    }
    auto delta = diff( current_.value, request.next );
    if( delta.empty() ) { return std::nullopt; }
    if( current_.at.revision == std::numeric_limits<counter>::max() ||
        current_.at.sequence == std::numeric_limits<counter>::max() ) {
        return std::unexpected( error::resource_limit );
    }
    auto type = classify( delta );
    auto event = public_event{{
            .sequence = current_.at.sequence + 1,
            .revision = current_.at.revision + 1,
            .type = std::move( type ),
            .cause = request.cause,
            .command = std::move( request.command ),
            .delta = std::move( delta ),
        }};
    if( !valid_event( event.value() ) ) { return std::unexpected( error::validation_failed ); }
    if( serialize_events( {.epoch = current_.at.epoch, .events = {event}} ).size() >
        maximum_event_bytes ) {
        return std::unexpected( error::resource_limit );
    }
    current_.value = std::move( request.next );
    current_.at.sequence = event.value().sequence;
    current_.at.revision = event.value().revision;
    return event;
}

auto event_stream::publish_message( message_request request ) -> std::expected<public_event, error>
{
    if( current_.at.sequence == std::numeric_limits<counter>::max() ) {
        return std::unexpected( error::resource_limit );
    }
    auto event = public_event{{
            .sequence = current_.at.sequence + 1,
            .revision = current_.at.revision,
            .type = "message.logged",
            .command = std::move( request.command ),
            .message = std::move( request.message ),
        }};
    if( !valid_event( event.value() ) ) { return std::unexpected( error::validation_failed ); }
    if( serialize_events( {.epoch = current_.at.epoch, .events = {event}} ).size() >
        maximum_event_bytes ) {
        return std::unexpected( error::resource_limit );
    }
    current_.at.sequence = event.value().sequence;
    remember( current_.log, *event.value().message );
    return event;
}

auto event_stream::publish_presentation( presentation_request request ) ->
std::expected<public_event, error>
{
    if( current_.at.sequence == std::numeric_limits<counter>::max() ) {
        return std::unexpected( error::resource_limit );
    }
    auto event = public_event{{
            .sequence = current_.at.sequence + 1,
            .revision = current_.at.revision,
            .type = request.fact.type,
            .command = std::move( request.command ),
            .presentation = std::move( request.fact ),
        }};
    if( !valid_event( event.value() ) ) { return std::unexpected( error::validation_failed ); }
    if( serialize_events( {.epoch = current_.at.epoch, .events = {event}} ).size() >
        maximum_event_bytes ) {
        return std::unexpected( error::resource_limit );
    }
    current_.at.sequence = event.value().sequence;
    return event;
}

auto event_stream::rebase( state_value next ) -> std::expected<void, error>
{
    if( const auto valid = validate_state( next ); !valid ) { return valid; }
    if( current_.at.revision == std::numeric_limits<counter>::max() ) {
        return std::unexpected( error::resource_limit );
    }
    current_.value = std::move( next );
    ++current_.at.revision;
    return {};
}

auto apply_event( snapshot &receiver, const std::string &epoch, const public_event &event )
-> std::expected<void, error>
{
    const auto &value = event.value();
    if( !valid_event( value ) ) { return std::unexpected( error::resync_required ); }
    if( epoch != receiver.at.epoch || receiver.at.sequence == std::numeric_limits<counter>::max() ||
        value.sequence != receiver.at.sequence + 1 ||
        value.revision != receiver.at.revision + ( value.delta.empty() ? 0 : 1 ) ) {
        return std::unexpected( error::resync_required );
    }
    auto next = receiver.value;
    if( const auto applied = apply( next, value.delta ); !applied ) { return applied; }
    receiver.value = std::move( next );
    if( value.message ) { remember( receiver.log, *value.message ); }
    receiver.at.sequence = value.sequence;
    receiver.at.revision = value.revision;
    return {};
}
auto apply_batch( snapshot &receiver, const event_batch &batch ) -> std::expected<void, error>
{
    auto candidate = receiver;
    for( const auto &event : batch.events ) {
        if( const auto applied = apply_event( candidate, batch.epoch, event ); !applied ) { return applied; }
    }
    receiver = std::move( candidate );
    return {};
}
} // namespace engine_client
