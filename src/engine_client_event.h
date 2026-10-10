#pragma once

#include "engine_client_contract.h"
#include "engine_client_presentation.h"
#include "loading_ui_client.h"

#include <expected>
#include <optional>
#include <string>
#include <vector>

namespace engine_client
{

/// Everything a client reconstructs: the current boundary plus what the avatar knows.
struct state_value {
    boundary_state interaction;
    world_state world;
};
struct gone_entity {
    std::string id = {};
    std::string reason = "lost_sight";
};
/// The only state-bearing part of an event. Applied in member order (design 3.4):
/// coverage, view, cells, forgotten, entities, gone, then avatar/environment/route/interaction replace.
struct changes {
    std::optional<bounds> coverage = std::nullopt;
    /// Replaces the clickable view.
    std::optional<bounds> view = std::nullopt;
    std::vector<cell> cells = {};
    std::vector<position> forgotten = {};
    std::vector<entity> entities = {};
    std::vector<gone_entity> gone = {};
    std::optional<avatar_value> avatar = std::nullopt;
    std::optional<environment_value> environment = std::nullopt;
    /// Replaces the whole route; empty clears it.
    std::optional<std::vector<position>> route = std::nullopt;
    std::optional<boundary_state> interaction = std::nullopt;
    /// The wire leaves out the `actions` of `interaction` when they equal the previous boundary's;
    /// a client keeps its own. Never set on a snapshot.
    bool actions_unchanged = false;
    auto empty() const -> bool;
};
/// The changes that turn `from` into `to`. A cell or entity that left perception is
/// republished (`remembered`) or listed as forgotten/gone, so nothing visible survives implicitly.
auto diff( const state_value &from, const state_value &to ) -> changes;
/// Fails without partial effects when the changes do not fit `value` (unknown id, cell outside
/// the coverage, duplicate entry).
auto apply( state_value &value, const changes &delta ) -> std::expected<void, error>;
auto same_state( const state_value &left, const state_value &right ) -> bool;

/// Supplied by the engine at the logical event boundary, never queried from a sink.
enum class disclosure { withheld, publish };
/// `message.logged`: one line of the native message log. A repeat of the last line is published
/// again with the same `id` and a higher `count`, replacing the line the client shows.
struct message_value {
    counter id = 0;
    std::string text = {};
    /// Native message type name: good, bad, mixed, warning, info, neutral or debug.
    std::string kind = {};
    /// Native color name of the type for a new message.
    std::string color = {};
    counter count = 1;
};
struct event_value {
    counter sequence = 0;
    counter revision = 0;
    std::string type = {};
    std::optional<counter> cause = std::nullopt;
    std::optional<std::string> command = std::nullopt;
    changes delta = {};
    /// Present exactly for `message.logged`, which changes no state.
    std::optional<message_value> message = std::nullopt;
    /// Present exactly for the presentation types (`projectile.moved`, `explosion.*`,
    /// `combat_text.shown`), which change no state.
    std::optional<presentation_value> presentation = std::nullopt;
};
/// Owned value with read-only access after construction.
class public_event
{
    public:
        explicit public_event( event_value value );
        auto value() const -> const event_value &; // *NOPAD*
    private:
        event_value value_;
};
struct event_batch {
    std::string epoch = {};
    std::vector<public_event> events = {};
};
struct snapshot {
    clock_point at = {};
    state_value value = {};
};

struct message_request {
    message_value message = {};
    std::optional<std::string> command = std::nullopt;
};
struct presentation_request {
    presentation_value fact = {};
    std::optional<std::string> command = std::nullopt;
};
struct publish_request {
    disclosure decision = disclosure::publish;
    state_value next = {};
    std::optional<std::string> command = std::nullopt;
    std::optional<counter> cause = std::nullopt;
};
/// The one ordered stream of an epoch. A failed or withheld publication consumes no
/// sequence, revision or ID and leaves the state unchanged.
class event_stream
{
    public:
        static auto create( std::string epoch, state_value initial ) -> std::expected<event_stream, error>;
        auto current() const -> const snapshot &; // *NOPAD*
        /// Nothing is published when the decision is withheld or the state did not change.
        /// `resync_required`: the state drops a coverage, avatar or environment, which no event
        /// can express; the caller rebases and tells subscribers to start over.
        auto publish( publish_request request ) -> std::expected<std::optional<public_event>, error>;
        /// A transient event: consumes a sequence, never a revision. resource_limit when it cannot fit a frame.
        auto publish_message( message_request request ) -> std::expected<public_event, error>;
        /// A transient animation fact: consumes a sequence, never a revision.
        auto publish_presentation( presentation_request request ) -> std::expected<public_event, error>;
        /// Engine-boundary recovery when an event cannot be encoded: adopt `next` and advance the
        /// revision only. Subscribers must be told `resync` and take a fresh snapshot.
        auto rebase( state_value next ) -> std::expected<void, error>;
    private:
        explicit event_stream( snapshot initial );
        snapshot current_;
};

/// Transactional reconstruction. Epoch, gap, duplicate, revision or changes violations leave the
/// receiver unchanged; an invalid suffix applies no prefix.
auto apply_event( snapshot &receiver, const std::string &epoch, const public_event &event )
-> std::expected<void, error>;
auto apply_batch( snapshot &receiver, const event_batch &batch ) -> std::expected<void, error>;

/// `type` for a published change set, e.g. `cells.seen`.
auto classify( const changes &delta ) -> std::string;

/// Params of `bn.events`.
auto serialize_events( const event_batch &batch ) -> std::string;
/// A snapshot as it goes on the wire: the `bn.subscribe` result, then one `bn.snapshot.part`
/// params value per part. Every piece fits maximum_inline_bytes; parts hold at most
/// cells_per_part cells and are split by size, so none is empty.
struct snapshot_wire {
    std::string header = {};
    std::vector<std::string> parts = {};
};
/// resource_limit when the header (entities, inventory) or a single cell cannot fit one piece.
auto serialize_snapshot( const snapshot &value ) -> std::expected<snapshot_wire, error>;
/// Params of `bn.loading`: one loading step, or `done` once the screen is gone.
auto serialize_loading( const std::string &epoch, const game_client::loading_progress &progress )
-> std::string;
auto serialize_resync( const std::string &epoch, std::string_view reason, counter lost_after )
-> std::string;

} // namespace engine_client
