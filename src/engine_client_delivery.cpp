#include "engine_client_delivery.h"

#include <algorithm>
#include <condition_variable>
#include <limits>
#include <mutex>
#include <utility>

namespace engine_client
{
namespace
{
auto valid_scope( const delivery_scope &scope ) -> bool
{
    return !scope.epoch.empty() && scope.epoch.size() <= 256;
}
struct queued_packet {
    delivery_packet packet;
    delivery_time admitted;
};
} // namespace

struct delivery_state {
    explicit delivery_state( delivery_options value ) : options( std::move( value ) ),
        queue( options.queue_events ), history( options.history_events ),
        latest( options.initial_clock ), delivered( latest.sequence ), acknowledged( latest.sequence ) {}

    delivery_options options;
    mutable std::mutex mutex;
    std::condition_variable wake;
    std::vector<queued_packet> queue;
    std::vector<delivery_packet> history;
    std::size_t queue_head = 0;
    std::size_t queued_count = 0;
    std::size_t queued_bytes = 0;
    std::size_t history_head = 0;
    std::size_t history_count = 0;
    std::size_t history_bytes = 0;
    delivery_packet reserved;
    delivery_time reserved_time;
    std::optional<delivery_time> last_time;
    std::size_t output_count = 0;
    std::size_t output_bytes = 0;
    std::uint64_t output_last = 0;
    std::size_t credit_events = 0;
    std::size_t credit_bytes = 0;
    std::uint64_t credit_serial = 0;
    delivery_clock latest;
    std::uint64_t delivered;
    std::uint64_t acknowledged;
    bool connected = true;

    auto occupied_count() const -> std::size_t {
        return queued_count + output_count + ( reserved ? 1 : 0 );
    }
    auto occupied_bytes() const -> std::size_t {
        return queued_bytes + output_bytes + ( reserved ? reserved->accounted_bytes() : 0 );
    }
    auto capacity_for( std::size_t bytes ) const -> bool {
        return !reserved && occupied_count() < options.queue_events &&
               bytes <= options.queue_bytes - occupied_bytes();
    }
    auto deadline() const -> std::optional<delivery_time> {
        if( queued_count == 0 ) {
            return std::nullopt;
        }
        const auto start = queue[queue_head].admitted;
        const auto interval = std::chrono::duration_cast<delivery_time::duration>( options.flush_interval );
        if( start.time_since_epoch() > delivery_time::duration::max() - interval ) {
            return delivery_time::max();
        }
        return start + interval;
    }
    auto disconnect_locked() -> void {
        connected = false;
        reserved.reset();
        // Release the subscriber backlog, but retain actual published history for reporting/recovery.
        while( queued_count != 0 ) {
            queue[queue_head].packet.reset();
            queue_head = ( queue_head + 1 ) % queue.size();
            --queued_count;
        }
        queued_bytes = 0;
        wake.notify_all();
    }
    auto clear_output() -> void {
        output_count = 0;
        output_bytes = 0;
        wake.notify_all();
    }
};

encoded_delivery_packet::encoded_delivery_packet( const encoded_packet_options &options ) :
    scope_( options.scope ), clock_( options.clock ), encoded_( options.encoded ),
    accounted_bytes_( options.accounted_bytes ) {}

auto encoded_delivery_packet::prepare( const encoded_packet_options &options )
-> std::expected<delivery_packet, delivery_error>
{
    if( !valid_scope( options.scope ) || options.encoded.empty() ||
        options.accounted_bytes < options.encoded.size() ) {
        return std::unexpected( delivery_error::invalid_packet );
    }
    // Construct/allocate the complete immutable value BEFORE attempting admission.
    return delivery_packet( new const encoded_delivery_packet( options ) );
}
auto encoded_delivery_packet::scope() const -> const delivery_scope & { return scope_; } // *NOPAD*
auto encoded_delivery_packet::clock() const -> delivery_clock { return clock_; }
auto encoded_delivery_packet::encoded() const -> const std::string & { return encoded_; } // *NOPAD*
auto encoded_delivery_packet::accounted_bytes() const -> std::size_t { return accounted_bytes_; }

delivery_reservation::delivery_reservation( std::shared_ptr<delivery_state> state ) noexcept :
    state_( std::move( state ) ) {}
delivery_reservation::delivery_reservation( delivery_reservation &&other ) noexcept = default;
auto delivery_reservation::operator=( delivery_reservation &&other ) noexcept
-> delivery_reservation & // *NOPAD*
{
    if( this != &other ) {
        cancel();
        state_ = std::move( other.state_ );
    }
    return *this;
}
delivery_reservation::~delivery_reservation() { cancel(); }
auto delivery_reservation::cancel() noexcept -> void
{
    if( auto state = std::move( state_ ) ) {
        const auto lock = std::lock_guard( state->mutex );
        state->reserved.reset();
        state->wake.notify_all();
    }
}
auto delivery_reservation::commit() noexcept -> std::expected<void, delivery_error>
{
    auto state = std::move( state_ );
    if( !state ) {
        return std::unexpected( delivery_error::invalid_packet );
    }
    const auto lock = std::lock_guard( state->mutex );
    if( !state->connected ) {
        return std::unexpected( delivery_error::disconnected );
    }
    const auto packet = std::move( state->reserved );
    if( !packet ) {
        return std::unexpected( delivery_error::invalid_packet );
    }
    const auto bytes = packet->accounted_bytes();
    auto &slot = state->queue[( state->queue_head + state->queued_count ) % state->queue.size()];
    slot.packet = packet;
    slot.admitted = state->reserved_time;
    ++state->queued_count;
    state->queued_bytes += bytes;
    state->last_time = state->reserved_time;
    // Eviction is prefix-only; disabled history never suppresses required gameplay delivery.
    if( !state->history.empty() ) {
        while( state->history_count == state->history.size() ||
               bytes > state->options.history_bytes - state->history_bytes ) {
            auto &oldest = state->history[state->history_head];
            state->history_bytes -= oldest->accounted_bytes();
            oldest.reset();
            state->history_head = ( state->history_head + 1 ) % state->history.size();
            --state->history_count;
        }
        state->history[( state->history_head + state->history_count ) % state->history.size()] = packet;
        ++state->history_count;
        state->history_bytes += bytes;
    }
    state->latest = packet->clock();
    state->wake.notify_all();
    return {};
}

delivery_output_batch::delivery_output_batch( std::shared_ptr<delivery_state> state,
        std::vector<delivery_packet> packets ) noexcept : state_( std::move( state ) ),
    packets_( std::move( packets ) ) {}
delivery_output_batch::delivery_output_batch( delivery_output_batch &&other ) noexcept = default;
auto delivery_output_batch::operator=( delivery_output_batch &&other ) noexcept
-> delivery_output_batch & // *NOPAD*
{
    if( this != &other ) {
        fail();
        state_ = std::move( other.state_ );
        packets_ = std::move( other.packets_ );
    }
    return *this;
}
delivery_output_batch::~delivery_output_batch() { fail(); }
auto delivery_output_batch::packets() const -> const std::vector<delivery_packet> & // *NOPAD*
{
    return packets_;
}
auto delivery_output_batch::fail() noexcept -> void
{
    if( auto state = std::move( state_ ) ) {
        const auto lock = std::lock_guard( state->mutex );
        state->disconnect_locked();
        state->clear_output();
    }
    packets_.clear();
}
auto delivery_output_batch::finish() noexcept -> std::expected<void, delivery_error>
{
    auto state = std::move( state_ );
    if( !state ) {
        return std::unexpected( delivery_error::invalid_packet );
    }
    const auto lock = std::lock_guard( state->mutex );
    const auto connected = state->connected;
    if( connected ) {
        state->delivered = state->output_last;
    }
    state->clear_output();
    packets_.clear();
    if( !connected ) {
        return std::unexpected( delivery_error::disconnected );
    }
    return {};
}

delivery_queue::delivery_queue( std::shared_ptr<delivery_state> state ) :
    state_( std::move( state ) ) {}
auto delivery_queue::create( delivery_options options )
-> std::expected<delivery_queue, delivery_error>
{
    const auto max_interval = std::chrono::duration_cast<std::chrono::milliseconds>(
                                  delivery_time::duration::max() );
    // Keep ring-index/count additions representable, even with malicious negotiated limits.
    const auto max_slots = std::numeric_limits<std::size_t>::max() / 2;
    const auto max_queue_slots = std::vector<queued_packet> {}.max_size();
    const auto max_history_slots = std::vector<delivery_packet> {}.max_size();
    if( !valid_scope( options.scope ) || options.batch_events == 0 || options.batch_bytes == 0 ||
        options.queue_events == 0 || options.queue_events > max_slots || options.queue_bytes == 0 ||
        options.history_events > max_slots ||
        ( ( options.history_events == 0 ) != ( options.history_bytes == 0 ) ) ||
        options.queue_events > max_queue_slots || options.history_events > max_history_slots ||
        options.batch_bytes > options.queue_bytes ||
        options.flush_interval.count() < 0 || options.flush_interval > max_interval ) {
        return std::unexpected( delivery_error::invalid_options );
    }
    return delivery_queue( std::make_shared<delivery_state>( std::move( options ) ) );
}
auto delivery_queue::reserve_delivery( delivery_packet packet, delivery_time now )
-> std::expected<delivery_reservation, delivery_error>
{
    const auto lock = std::lock_guard( state_->mutex );
    if( !state_->connected ) {
        return std::unexpected( delivery_error::disconnected );
    }
    if( !packet ) {
        return std::unexpected( delivery_error::invalid_packet );
    }
    if( packet->scope() != state_->options.scope ) {
        return std::unexpected( delivery_error::wrong_scope );
    }
    if( state_->reserved ) {
        return std::unexpected( delivery_error::busy );
    }
    const auto clock = packet->clock();
    if( state_->latest.sequence == std::numeric_limits<std::uint64_t>::max() ||
        clock.sequence != state_->latest.sequence + 1 || clock.revision < state_->latest.revision ||
        ( state_->last_time && now < *state_->last_time ) ) {
        return std::unexpected( delivery_error::bad_clock );
    }
    const auto bytes = packet->accounted_bytes();
    if( bytes > state_->options.batch_bytes ||
        ( !state_->history.empty() && bytes > state_->options.history_bytes ) ) {
        return std::unexpected( delivery_error::too_large );
    }
    if( !state_->capacity_for( bytes ) ) {
        return std::unexpected( delivery_error::full );
    }
    state_->reserved = std::move( packet );
    state_->reserved_time = now;
    return delivery_reservation( state_ );
}
auto delivery_queue::grant_credit( const delivery_credit &credit )
-> std::expected<void, delivery_error>
{
    const auto lock = std::lock_guard( state_->mutex );
    if( credit.scope != state_->options.scope ) {
        return std::unexpected( delivery_error::wrong_scope );
    }
    if( !state_->connected ) {
        return std::unexpected( delivery_error::disconnected );
    }
    if( state_->credit_serial == std::numeric_limits<std::uint64_t>::max() ||
        credit.serial != state_->credit_serial + 1 ) {
        return std::unexpected( delivery_error::duplicate );
    }
    if( ( credit.events == 0 && credit.bytes == 0 ) ||
        credit.events > state_->options.queue_events - state_->credit_events ||
        credit.bytes > state_->options.queue_bytes - state_->credit_bytes ) {
        return std::unexpected( delivery_error::overgrant );
    }
    state_->credit_events += credit.events;
    state_->credit_bytes += credit.bytes;
    state_->credit_serial = credit.serial;
    state_->wake.notify_all();
    return {};
}
auto delivery_queue::acknowledge( const delivery_ack &ack )
-> std::expected<void, delivery_error>
{
    const auto lock = std::lock_guard( state_->mutex );
    if( ack.scope != state_->options.scope ) {
        return std::unexpected( delivery_error::wrong_scope );
    }
    if( !state_->connected ) {
        return std::unexpected( delivery_error::disconnected );
    }
    if( ack.sequence <= state_->acknowledged || ack.sequence > state_->delivered ) {
        return std::unexpected( delivery_error::invalid_ack );
    }
    state_->acknowledged = ack.sequence;
    return {};
}
auto delivery_queue::take_flush_batch( const delivery_flush &flush )
-> std::expected<std::optional<delivery_output_batch>, delivery_error>
{
    const auto lock = std::lock_guard( state_->mutex );
    if( !state_->connected ) {
        return std::unexpected( delivery_error::disconnected );
    }
    if( state_->output_count != 0 ) {
        return std::unexpected( delivery_error::busy );
    }
    if( state_->queued_count == 0 ) {
        return std::nullopt;
    }
    const auto full = state_->queued_count >= state_->options.batch_events ||
                      state_->queued_bytes >= state_->options.batch_bytes ||
                      state_->occupied_count() == state_->options.queue_events ||
                      state_->occupied_bytes() == state_->options.queue_bytes;
    if( !flush.force && !full && flush.now < *state_->deadline() ) {
        return std::nullopt;
    }
    auto packets = std::vector<delivery_packet> {};
    // All throwing work precedes queue/credit mutation. Exactly one output lease is permitted.
    packets.reserve( std::min( state_->queued_count, state_->options.batch_events ) );
    auto bytes = std::size_t{ 0 };
    while( packets.size() < state_->queued_count && packets.size() < state_->options.batch_events &&
           packets.size() < state_->credit_events ) {
        const auto &packet = state_->queue[( state_->queue_head + packets.size() ) %
                                                                                 state_->queue.size()].packet;
        const auto next_bytes = packet->accounted_bytes();
        if( next_bytes > state_->options.batch_bytes - bytes ||
            next_bytes > state_->credit_bytes - bytes ) {
            break;
        }
        packets.push_back( packet );
        bytes += next_bytes;
    }
    if( packets.empty() ) {
        return std::nullopt;
    }
    for( const auto &packet : packets ) {
        state_->queue[state_->queue_head].packet.reset();
        state_->queue_head = ( state_->queue_head + 1 ) % state_->queue.size();
        state_->output_last = packet->clock().sequence;
    }
    state_->queued_count -= packets.size();
    state_->queued_bytes -= bytes;
    state_->credit_events -= packets.size();
    state_->credit_bytes -= bytes;
    state_->output_count = packets.size();
    state_->output_bytes = bytes;
    return delivery_output_batch( state_, std::move( packets ) );
}
auto delivery_queue::resume_from( const delivery_ack &cursor ) const
-> std::expected<delivery_resume, delivery_error>
{
    const auto lock = std::lock_guard( state_->mutex );
    if( cursor.scope != state_->options.scope ) {
        return std::unexpected( delivery_error::wrong_scope );
    }
    if( cursor.sequence > state_->latest.sequence ) {
        return std::unexpected( delivery_error::invalid_ack );
    }
    auto result = delivery_resume{ .next_sequence = cursor.sequence };
    if( state_->history_count != 0 ) {
        result.earliest_retained = state_->history[state_->history_head]->clock().sequence;
        result.latest_retained = state_->history[( state_->history_head + state_->history_count - 1 ) %
                                                                                      state_->history.size()]->clock().sequence;
    }
    if( cursor.sequence == state_->latest.sequence ) {
        result.coverage = delivery_coverage::complete;
        return result;
    }
    // Subtraction avoids sequence+1 overflow. History is a contiguous retained suffix.
    if( !result.earliest_retained || cursor.sequence < *result.earliest_retained - 1 ) {
        return result;
    }
    // Admission is contiguous and packets are factory-only immutable. Subtract BEFORE converting
    // to a ring offset; neither cursor+1 nor a scan of the already consumed prefix is needed.
    const auto first_offset = cursor.sequence - ( *result.earliest_retained - 1 );
    if( first_offset >= state_->history_count ) {
        return result;
    }
    const auto start = static_cast<std::size_t>( first_offset );
    const auto limit = std::min( { state_->history_count - start, state_->options.batch_events,
                                   state_->options.queue_events } );
    result.coverage = delivery_coverage::complete;
    result.packets.reserve( limit );
    auto bytes = std::size_t{ 0 };
    for( auto offset = start; offset < state_->history_count &&
         result.packets.size() < limit; ++offset ) {
        const auto &packet = state_->history[( state_->history_head + offset ) % state_->history.size()];
        if( packet->accounted_bytes() > state_->options.batch_bytes - bytes ) {
            break;
        }
        result.packets.push_back( packet );
        bytes += packet->accounted_bytes();
        result.next_sequence = packet->clock().sequence;
    }
    result.more = result.next_sequence < state_->latest.sequence;
    return result;
}
auto delivery_queue::status() const -> delivery_status
{
    const auto lock = std::lock_guard( state_->mutex );
    auto result = delivery_status{
        .connected = state_->connected,
        .reserved = static_cast<bool>( state_->reserved ),
        .output_pending = state_->output_count != 0,
        .queue_events = state_->occupied_count(),
        .queue_bytes = state_->occupied_bytes(),
        .history_events = state_->history_count,
        .history_bytes = state_->history_bytes,
        .credit_events = state_->credit_events,
        .credit_bytes = state_->credit_bytes,
        .latest_sequence = state_->latest.sequence,
        .latest_revision = state_->latest.revision,
        .delivered_sequence = state_->delivered,
        .acknowledged_sequence = state_->acknowledged,
        .flush_deadline = state_->deadline()
    };
    if( state_->history_count != 0 ) {
        result.earliest_retained = state_->history[state_->history_head]->clock().sequence;
        result.latest_retained = state_->history[( state_->history_head + state_->history_count - 1 ) %
                                                                                      state_->history.size()]->clock().sequence;
    }
    return result;
}
auto delivery_queue::disconnect() noexcept -> void
{
    const auto lock = std::lock_guard( state_->mutex );
    state_->disconnect_locked();
}
auto delivery_queue::wait_for_capacity( std::size_t accounted_bytes )
-> std::expected<void, delivery_error>
{
    auto lock = std::unique_lock( state_->mutex );
    if( accounted_bytes == 0 || accounted_bytes > state_->options.batch_bytes ||
        ( !state_->history.empty() && accounted_bytes > state_->options.history_bytes ) ) {
        return std::unexpected( delivery_error::too_large );
    }
    state_->wake.wait( lock, [&] { return !state_->connected || state_->capacity_for( accounted_bytes ); } );
    if( !state_->connected ) {
        return std::unexpected( delivery_error::disconnected );
    }
    return {};
}

} // namespace engine_client
