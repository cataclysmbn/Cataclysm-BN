#pragma once

#include <chrono>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace engine_client
{

/// Transport identity only; the engine owns epochs and all publication clocks.
struct delivery_scope {
    std::string epoch = {};
    std::uint64_t stream = 0;
    auto operator<=>( const delivery_scope & ) const = default; // *NOPAD*
};
struct delivery_clock {
    std::uint64_t sequence = 0;
    std::uint64_t revision = 0;
};
enum class delivery_error {
    invalid_options, invalid_packet, wrong_scope, disconnected, busy, full,
    too_large, bad_clock, duplicate, overgrant, invalid_ack
};
struct encoded_packet_options {
    delivery_scope scope = {};
    delivery_clock clock = {};
    std::string_view encoded = {};
    /// Caller charges ALL framing/envelope/batch overhead here, conservatively per event.
    /// No assumption that transport byte budgets equal total allocator/metadata memory.
    std::size_t accounted_bytes = 0;
};
/// Immutable, owned encoding. No JSON/domain union, borrowed world data, or renderer handles.
class encoded_delivery_packet
{
    public:
        encoded_delivery_packet( const encoded_delivery_packet &/*other*/ ) = delete;
        encoded_delivery_packet( encoded_delivery_packet &&/*other*/ ) = delete;
        auto operator=( const encoded_delivery_packet &/*other*/ ) -> encoded_delivery_packet & =
            delete; // *NOPAD*
        auto operator=( encoded_delivery_packet &&
                        /*other*/ ) -> encoded_delivery_packet & = delete; // *NOPAD*
        static auto prepare( const encoded_packet_options &options )
        -> std::expected<std::shared_ptr<const encoded_delivery_packet>, delivery_error>;
        auto scope() const -> const delivery_scope &; // *NOPAD*
        auto clock() const -> delivery_clock;
        auto encoded() const -> const std::string &; // *NOPAD*
        auto accounted_bytes() const -> std::size_t;
    private:
        explicit encoded_delivery_packet( const encoded_packet_options &options );
        delivery_scope scope_;
        delivery_clock clock_;
        std::string encoded_;
        std::size_t accounted_bytes_;
};
using delivery_packet = std::shared_ptr<const encoded_delivery_packet>;
using delivery_time = std::chrono::steady_clock::time_point;
struct delivery_options {
    delivery_scope scope = {};
    delivery_clock initial_clock = {};
    std::size_t batch_events = 8;
    std::size_t batch_bytes = 262144;
    std::size_t queue_events = 1024;
    std::size_t queue_bytes = 8 * 1024 * 1024;
    /// Paired zero event/byte limits disable retention, not gameplay delivery.
    std::size_t history_events = 4096;
    std::size_t history_bytes = 32 * 1024 * 1024;
    std::chrono::milliseconds flush_interval = std::chrono::milliseconds( 16 );
};
struct delivery_credit {
    delivery_scope scope;
    /// Strictly contiguous control serial, starts at 1. Ack never implicitly grants credit.
    std::uint64_t serial = 0;
    std::size_t events = 0;
    std::size_t bytes = 0;
};
struct delivery_ack {
    delivery_scope scope;
    std::uint64_t sequence = 0;
};
struct delivery_flush {
    delivery_time now;
    /// Logical checkpoint, completion, or shutdown; never overrides credit/capacity.
    bool force = false;
};
struct delivery_status {
    bool connected = true;
    bool reserved = false;
    bool output_pending = false;
    /// Includes queued, reserved, AND the single output batch until successful finish.
    std::size_t queue_events = 0;
    std::size_t queue_bytes = 0;
    std::size_t history_events = 0;
    std::size_t history_bytes = 0;
    std::size_t credit_events = 0;
    std::size_t credit_bytes = 0;
    std::uint64_t latest_sequence = 0;
    /// Trusted revision at latest_sequence, for sink admission against the installed endpoint.
    std::uint64_t latest_revision = 0;
    std::uint64_t delivered_sequence = 0;
    std::uint64_t acknowledged_sequence = 0;
    std::optional<std::uint64_t> earliest_retained = std::nullopt;
    std::optional<std::uint64_t> latest_retained = std::nullopt;
    std::optional<delivery_time> flush_deadline = std::nullopt;
};
struct delivery_state;
/// Only one producer reservation at a time. Destruction cancels without consuming a clock.
/// A disconnect between reservation and commit makes commit fail, still without publication.
class delivery_reservation
{
    public:
        delivery_reservation( delivery_reservation &&other ) noexcept;
        auto operator=( delivery_reservation &&other ) noexcept -> delivery_reservation &; // *NOPAD*
        ~delivery_reservation();
        auto commit() noexcept -> std::expected<void, delivery_error>;
        auto cancel() noexcept -> void;
    private:
        friend class delivery_queue;
        explicit delivery_reservation( std::shared_ptr<delivery_state> state ) noexcept;
        std::shared_ptr<delivery_state> state_;
};
/// Single I/O owner's immutable values. Hold this lease across partial writes.
/// Finish ONLY after the entire output succeeds. Abandonment/failure disconnects, not completion.
class delivery_output_batch
{
    public:
        delivery_output_batch( delivery_output_batch &&other ) noexcept;
        auto operator=( delivery_output_batch &&other ) noexcept -> delivery_output_batch &; // *NOPAD*
        ~delivery_output_batch();
        auto packets() const -> const std::vector<delivery_packet> &; // *NOPAD*
        auto finish() noexcept -> std::expected<void, delivery_error>;
        auto fail() noexcept -> void;
    private:
        friend class delivery_queue;
        delivery_output_batch( std::shared_ptr<delivery_state> state,
                               std::vector<delivery_packet> packets ) noexcept;
        std::shared_ptr<delivery_state> state_;
        std::vector<delivery_packet> packets_;
};
enum class delivery_coverage { complete, gapped };
struct delivery_resume {
    delivery_coverage coverage = delivery_coverage::gapped;
    std::optional<std::uint64_t> earliest_retained = std::nullopt;
    std::optional<std::uint64_t> latest_retained = std::nullopt;
    /// Bounded first page; complete describes coverage of ALL events after requested cursor.
    /// Pages are also capped by queue capacity, even when the batch ceiling is larger.
    /// Gapped returns no misleading suffix. Snapshots cannot recover missed transient effects.
    std::vector<delivery_packet> packets = {};
    std::uint64_t next_sequence = 0;
    bool more = false;
};
/// Thread-safe transport-value helper; no stream writes, world commands, RNG, or clock minting.
/// Fixed ring slots + one reservation/output lease; credit/ack controls allocate no backlog.
/// Preparation and batch/page allocation may throw BEFORE mutation. Commit does not allocate.
/// Returned packets/pages retained by callers are outside this helper's resource ownership.
class delivery_queue
{
    public:
        static auto create( delivery_options options ) -> std::expected<delivery_queue, delivery_error>;
        auto reserve_delivery( delivery_packet packet, delivery_time now )
        -> std::expected<delivery_reservation, delivery_error>;
        auto grant_credit( const delivery_credit &credit ) -> std::expected<void, delivery_error>;
        auto acknowledge( const delivery_ack &ack ) -> std::expected<void, delivery_error>;
        auto take_flush_batch( const delivery_flush &flush )
        -> std::expected<std::optional<delivery_output_batch>, delivery_error>;
        auto resume_from( const delivery_ack &cursor ) const
        -> std::expected<delivery_resume, delivery_error>;
        auto status() const -> delivery_status;
        auto disconnect() noexcept -> void;
        /// Future safe producer checkpoint only. Never pumps commands. Disconnect/cancel/finish wake it.
        auto wait_for_capacity( std::size_t accounted_bytes ) -> std::expected<void, delivery_error>;
    private:
        explicit delivery_queue( std::shared_ptr<delivery_state> state );
        std::shared_ptr<delivery_state> state_;
};

} // namespace engine_client
