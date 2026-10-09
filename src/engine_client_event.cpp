#include "engine_client_event.h"

#include <limits>
#include <type_traits>
#include <utility>

namespace engine_client
{
namespace
{
static_assert( std::is_nothrow_move_assignable_v<state_value> );
static_assert( std::is_nothrow_move_assignable_v<snapshot> );
static_assert( std::is_nothrow_constructible_v < std::expected<std::optional<public_event>, error>,
               public_event && > );
static_assert( std::is_nothrow_constructible_v < std::expected<std::optional<snapshot>, error>,
               snapshot && > );

auto valid_event( const event_value &value ) -> std::expected<void, error>
{
    if( value.session_epoch.empty() || value.session_epoch.size() > maximum_id_bytes ||
        value.event_id.empty() || value.event_id.size() > maximum_id_bytes || value.public_sequence == 0 ||
        value.base_state_revision == std::numeric_limits<counter>::max() ||
        value.state_revision != value.base_state_revision + 1 ||
        ( value.payload.cause_sequence && ( *value.payload.cause_sequence == 0 ||
                                            *value.payload.cause_sequence >= value.public_sequence ) ) ||
        ( value.payload.command_id && ( value.payload.command_id->empty() ||
                                        value.payload.command_id->size() > maximum_id_bytes ) ) ) {
        return std::unexpected( error::resync_required );
    }
    if( value.payload.display ) {
        const auto &display = *value.payload.display;
        if( display.group_id.empty() || display.group_id.size() > maximum_id_bytes ||
            display.count <= 0 || display.ordinal < 0 ||
            display.ordinal >= display.count || display.duration_ms < 0 ) {
            return std::unexpected( error::validation_failed );
        }
    }
    return validate_state( value.payload.state );
}
} // namespace

public_event::public_event( event_value value ) : value_( std::move( value ) ) {}
auto public_event::value() const -> const event_value & { return value_; } // *NOPAD*
auto null_event_sink::publish( public_event /*event*/ ) -> std::expected<void, error> { return {}; }
auto recording_event_sink::publish( public_event event ) -> std::expected<void, error>
{
    if( const auto valid = valid_event( event.value() ); !valid ) { return valid; }
    if( !events_.empty() && ( events_.back().value().session_epoch != event.value().session_epoch ||
                              events_.back().value().public_sequence == std::numeric_limits<counter>::max() ||
                              event.value().public_sequence != events_.back().value().public_sequence + 1 ||
                              event.value().base_state_revision != events_.back().value().state_revision ) ) {
        return std::unexpected( error::resync_required );
    }
    if( events_.size() == maximum_inline_events ) { return std::unexpected( error::resource_limit ); }
    auto candidate = event_batch{};
    candidate.events = events_;
    candidate.events.push_back( event );
    candidate.first_sequence = candidate.events.front().value().public_sequence;
    candidate.last_sequence = candidate.events.back().value().public_sequence;
    if( serialize_batch( candidate ).size() > maximum_inline_bytes ) {
        return std::unexpected( error::resource_limit );
    }
    events_.push_back( std::move( event ) );
    return {};
}
auto recording_event_sink::events() const -> const std::vector<public_event> & { return events_; } // *NOPAD*
auto recording_event_sink::drain() -> event_batch
{
    auto result = event_batch{};
    if( !events_.empty() ) {
        result.first_sequence = events_.front().value().public_sequence;
        result.last_sequence = events_.back().value().public_sequence;
    }
    result.events = std::move( events_ );
    events_.clear();
    return result;
}
event_stream::event_stream( snapshot initial ) : current_( std::move( initial ) ) {}
auto event_stream::create( snapshot initial ) -> std::expected<event_stream, error>
{
    if( initial.session_epoch.empty() || initial.session_epoch.size() > maximum_id_bytes ) {
        return std::unexpected( error::validation_failed );
    }
    if( const auto valid = validate_state( initial.state ); !valid ) {
        return std::unexpected( valid.error() );
    }
    if( serialize_snapshot( initial ).size() > maximum_inline_bytes ) {
        return std::unexpected( error::resource_limit );
    }
    return event_stream{std::move( initial )};
}
auto event_stream::current_snapshot() const -> snapshot { return current_; }
auto event_stream::project_snapshot( state_value candidate ) const -> std::expected<snapshot, error>
{
    if( const auto valid = validate_state( candidate ); !valid ) { return std::unexpected( valid.error() ); }
    if( !same_boundary_state( current_.state, candidate ) ||
        ( current_.state.view == candidate.view && !same_state( current_.state, candidate ) ) ) {
        return std::unexpected( error::stale_boundary );
    }
    auto result = current_;
    result.state = std::move( candidate );
    if( serialize_snapshot( result ).size() > maximum_inline_bytes ) { return std::unexpected( error::resource_limit ); }
    return result;
}
auto project_event( const public_event &event,
                    state_value candidate ) -> std::expected<public_event, error>
{
    if( const auto valid = valid_event( event.value() ); !valid ) { return std::unexpected( valid.error() ); }
    if( const auto valid = validate_state( candidate ); !valid ) { return std::unexpected( valid.error() ); }
    const auto &published = event.value().payload.state;
    if( !same_boundary_state( published, candidate ) ||
        ( published.view == candidate.view && !same_state( published, candidate ) ) ) {
        return std::unexpected( error::stale_boundary );
    }
    auto value = event.value();
    value.payload.state = std::move( candidate );
    auto result = public_event{std::move( value )};
    if( serialize_event( result ).size() > maximum_inline_bytes ) { return std::unexpected( error::resource_limit ); }
    return result;
}
auto event_stream::replace( const disclosure decision, replacement candidate, event_sink &sink )
-> std::expected<std::optional<public_event>, error>
{
    if( decision == disclosure::withheld ) { return std::nullopt; }
    if( const auto valid = validate_state( candidate.state ); !valid ) { return std::unexpected( valid.error() ); }
    if( candidate.cause_sequence && ( *candidate.cause_sequence == 0 ||
                                      *candidate.cause_sequence > current_.through_public_sequence ) ) {
        return std::unexpected( error::validation_failed );
    }
    if( current_.state.view != candidate.state.view ) {
        if( same_boundary_state( current_.state, candidate.state ) ) { return std::nullopt; }
        return std::unexpected( error::stale_boundary );
    }
    if( same_state( current_.state, candidate.state ) ) { return std::nullopt; }
    if( current_.state_revision == std::numeric_limits<counter>::max() ||
        current_.through_public_sequence == std::numeric_limits<counter>::max() ) {
        return std::unexpected( error::resource_limit );
    }
    const auto sequence = current_.through_public_sequence + 1;
    auto event = public_event{{
            .session_epoch = current_.session_epoch,
            .event_id = game_client::opaque_interaction_id( "event", {current_.session_epoch, std::to_string( sequence )} ),
            .public_sequence = sequence,
            .base_state_revision = current_.state_revision,
            .state_revision = current_.state_revision + 1,
            .payload = std::move( candidate ),
        }};
    if( const auto valid = valid_event( event.value() ); !valid ) { return std::unexpected( valid.error() ); }
    auto batch = event_batch{.first_sequence = sequence, .last_sequence = sequence, .events = {event}};
    if( serialize_batch( batch ).size() > maximum_inline_bytes ) { return std::unexpected( error::resource_limit ); }
    auto next_state = event.value().payload.state;
    if( const auto accepted = sink.publish( event ); !accepted ) { return std::unexpected( accepted.error() ); }
    current_.state = std::move( next_state );
    current_.state_revision = event.value().state_revision;
    current_.through_public_sequence = sequence;
    return event;
}
auto event_stream::resynchronize( const disclosure decision, state_value candidate )
-> std::expected<std::optional<snapshot>, error>
{
    if( decision == disclosure::withheld ) { return std::nullopt; }
    if( const auto valid = validate_state( candidate ); !valid ) { return std::unexpected( valid.error() ); }
    if( current_.state.view != candidate.view ) {
        return std::unexpected( error::stale_boundary ); // Reference view is fixed; use pure projection.
    }
    if( same_state( current_.state, candidate ) ) { return current_; }
    if( current_.state_revision == std::numeric_limits<counter>::max() ) {
        return std::unexpected( error::resource_limit );
    }
    auto next = snapshot{
        .session_epoch = current_.session_epoch,
        .state_revision = current_.state_revision + 1,
        .through_public_sequence = current_.through_public_sequence,
        .state = std::move( candidate ),
    };
    if( serialize_snapshot( next ).size() > maximum_inline_bytes ) {
        return std::unexpected( error::resource_limit );
    }
    auto result = next;
    current_ = std::move( next );
    return result;
}
auto apply_event( snapshot &receiver, const public_event &event ) -> std::expected<void, error>
{
    const auto &value = event.value();
    if( const auto valid = valid_event( value ); !valid ) { return valid; }
    if( serialize_event( event ).size() > maximum_inline_bytes ) {
        return std::unexpected( error::resource_limit );
    }
    if( value.session_epoch != receiver.session_epoch ||
        value.payload.state.view != receiver.state.view ||
        receiver.through_public_sequence == std::numeric_limits<counter>::max() ||
        value.public_sequence != receiver.through_public_sequence + 1 ||
        value.base_state_revision != receiver.state_revision ||
        same_state( receiver.state, value.payload.state ) ) {
        return std::unexpected( error::resync_required );
    }
    auto next_state = value.payload.state;
    receiver.state = std::move( next_state );
    receiver.state_revision = value.state_revision;
    receiver.through_public_sequence = value.public_sequence;
    return {};
}
auto apply_batch( snapshot &receiver, const event_batch &batch ) -> std::expected<void, error>
{
    if( batch.events.empty() ) {
        if( batch.first_sequence || batch.last_sequence ) { return std::unexpected( error::resync_required ); }
        return {};
    }
    if( batch.events.size() > maximum_inline_events ||
        serialize_batch( batch ).size() > maximum_inline_bytes ) {
        return std::unexpected( error::resource_limit );
    }
    if( !batch.first_sequence || !batch.last_sequence ||
        *batch.first_sequence != batch.events.front().value().public_sequence ||
        *batch.last_sequence != batch.events.back().value().public_sequence ) {
        return std::unexpected( error::resync_required );
    }
    auto candidate = receiver;
    for( const auto &event : batch.events ) {
        if( const auto applied = apply_event( candidate, event ); !applied ) { return applied; }
    }
    receiver = std::move( candidate );
    return {};
}
} // namespace engine_client
