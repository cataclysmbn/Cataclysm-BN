#pragma once

#include "engine_client_contract.h"

#include <expected>
#include <optional>
#include <string>
#include <vector>

namespace engine_client
{

/// Supplied by the engine at the logical event boundary, never queried from a sink.
enum class disclosure { withheld, publish };
struct presentation {
    std::string group_id = {};
    int ordinal = 0;
    int count = 1;
    int duration_ms = 0;
};
struct replacement {
    state_value state;
    std::optional<std::string> command_id = std::nullopt;
    std::optional<counter> cause_sequence = std::nullopt;
    std::optional<presentation> display = std::nullopt;
};
struct event_value {
    std::string session_epoch = {};
    std::string event_id = {};
    counter public_sequence = 0;
    counter base_state_revision = 0;
    counter state_revision = 0;
    replacement payload;
};
/// Owned value with read-only access after construction. No world/renderer handles or borrowed data.
class public_event
{
    public:
        explicit public_event( event_value value );
        auto value() const -> const event_value &; // *NOPAD*
    private:
        event_value value_;
};
class event_sink
{
    public:
        virtual ~event_sink() = default;
        /// Failure must not consume/store a prefix. Sink ownership never grants disclosure.
        virtual auto publish( public_event event ) -> std::expected<void, error> = 0;
};
class null_event_sink final : public event_sink
{
    public:
        auto publish( public_event event ) -> std::expected<void, error> override;
};
struct event_batch {
    std::optional<counter> first_sequence = std::nullopt;
    std::optional<counter> last_sequence = std::nullopt;
    std::vector<public_event> events = {};
};
/// Bounded in-process recorder, not transport history. Drain before accepting another batch.
class recording_event_sink final : public event_sink
{
    public:
        auto publish( public_event event ) -> std::expected<void, error> override;
        auto events() const -> const std::vector<public_event> &; // *NOPAD*
        auto drain() -> event_batch;
    private:
        std::vector<public_event> events_ = {};
};
/// Publishes complete replacement deltas; hidden candidates are rejected before IDs/counters.
/// A failed publication leaves state/counters unchanged: retry or explicitly resynchronize.
/// Reference projection is fixed for the stream lifetime, including boundary changes/recovery.
class event_stream
{
    public:
        static auto create( snapshot initial ) -> std::expected<event_stream, error>;
        auto current_snapshot() const -> snapshot;
        /// Pure bounded projection of this committed boundary. No counters, epoch or events change.
        /// Supply an owned P1-approved capture from the same boundary, not a new stream per page.
        auto project_snapshot( state_value candidate ) const -> std::expected<snapshot, error>;
        auto replace( disclosure decision, replacement candidate, event_sink &sink )
        -> std::expected<std::optional<public_event>, error>;
        /// Engine-boundary recovery for this state-only scope, never a passive query.
        /// Commits a bounded snapshot without an event; receiver MUST be told resync_required.
        /// Advances a changed revision, not sequence/epoch. Cannot recover required presentation.
        auto resynchronize( disclosure decision, state_value candidate )
        -> std::expected<std::optional<snapshot>, error>;
    private:
        explicit event_stream( snapshot initial );
        snapshot current_;
};
/// Pure page-specific copy of a published boundary event, with the same public identity/clocks.
/// Capture while that boundary is valid; retain only owned bounded values for delivery.
/// No event registry, publication or reconstruction history is allocated here.
auto project_event( const public_event &event, state_value candidate )
-> std::expected<public_event, error>;
/// Transactional reconstruction. Epoch/gap/base/projection violations leave the receiver unchanged.
auto apply_event( snapshot &receiver, const public_event &event ) -> std::expected<void, error>;
auto apply_batch( snapshot &receiver, const event_batch &batch ) -> std::expected<void, error>;
auto serialize_event( const public_event &event ) -> std::string;
auto serialize_batch( const event_batch &batch ) -> std::string;
/// Adapter-owned inline result. No result/history accumulation in the core.
/// Oversize or incoherent responses fail atomically; return explicit resync metadata instead.
auto serialize_command_response( const command_result &command, const event_batch &batch )
-> std::expected<std::string, error>;

} // namespace engine_client
