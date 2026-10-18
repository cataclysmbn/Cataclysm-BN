#pragma once

#include "engine_client_contract.h"
#include "engine_client_presentation.h"
#include "loading_ui_client.h"
#include "localised_text.h"

#include <expected>
#include <optional>
#include <string>
#include <string_view>
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
    auto operator<=>( const gone_entity & ) const = default; // *NOPAD*
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
    /// The members of `avatar` that equal the previous avatar's and are not empty: the wire leaves
    /// them out and names them in `keep`, so a step does not resend the sidebar and the stats.
    std::vector<std::string> avatar_kept = {};
    auto empty() const -> bool;
};
/// The changes that turn `from` into `to`. A cell or entity that left perception is
/// republished (`remembered`) or listed as forgotten/gone, so nothing visible survives implicitly.
auto diff( const state_value &from, const state_value &to ) -> changes;
/// The changes that turn `from` into the state `patch` captures: the same as `diff` of the state it
/// stands for, found by comparing only the cells it names.
auto diff( const state_value &from, const boundary_state &interaction,
           const world_patch &patch ) -> changes;
/// The members of `after` the wire leaves out of an avatar that replaces `before`: those that equal
/// `before`'s and are not empty (`changes::avatar_kept`).
auto kept_members( const avatar_value &before,
                   const avatar_value &after ) -> std::vector<std::string>;
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
    /// The game turn the line was logged on, or last repeated on.
    counter turn = 0;
    /// The line the way the native log prints it: `text`, with the native repeat suffix from `count` 2 on.
    std::string label = {};
    /// Set on the copy a connection reads of a line that is not in its own locale: the BN language id the line is in.
    std::string lang = {};
    /// The untranslated format and values of a converted site: `text` and `label` then go out as LocalisedStrings
    /// the client renders in its own language, and `lang` is never set.
    std::optional<localised_text> localised = std::nullopt;
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
        /// The event as the sole event of its own `bn.events` params, serialized once when it was
        /// published; empty for an event that never went through a stream.
        auto wire() const -> const std::string &; // *NOPAD*
        /// The event's own value inside `wire`, which a batch of several events takes as it is; empty with it.
        auto element() const -> std::string_view;
        /// The event of `value` as the stream of `epoch` would seal it; none when it would not fit a frame.
        static auto sealed( event_value value, const std::string &epoch ) -> std::optional<public_event>;
    private:
        friend class event_stream;
        /// Serializes the event once for the wire; false when it would not fit a frame in the form any
        /// connection reads. Only an interaction change differs between forms: 0.2 adds the screen stack.
        auto seal( const std::string &epoch ) -> bool;
        event_value value_;
        std::string wire_;
        /// Where the event's value begins in `wire_`.
        std::size_t element_at_ = 0;
};
struct event_batch {
    std::string epoch = {};
    std::vector<public_event> events = {};
};
struct snapshot {
    clock_point at = {};
    state_value value = {};
    /// The newest `maximum_log_lines` message lines, oldest first. Not state: lines take no
    /// revision, but a receiver that applies the stream's `message.logged` events holds exactly
    /// this log, so a fresh subscribe equals it.
    std::vector<message_value> log = {};
};
/// Adds a line to a log: a repeat replaces the line with the same id, the oldest line falls off.
auto remember( std::vector<message_value> &log, const message_value &line ) -> void;

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
/// A publication whose next state is given as a patch over the current one.
struct patch_request {
    boundary_state interaction = {};
    world_patch patch = {};
    std::optional<std::string> command = std::nullopt;
};
/// The one ordered stream of an epoch. A failed or withheld publication consumes no
/// sequence, revision or ID and leaves the state unchanged.
class event_stream
{
    public:
        static auto create( std::string epoch, state_value initial ) -> std::expected<event_stream, error>;
        auto current() const -> const snapshot &; // *NOPAD*
        /// Puts lines the game wrote before this stream existed into its log: a snapshot carries them,
        /// and they take no sequence, since no subscriber can have missed an event.
        auto seed_log( std::vector<message_value> lines ) -> void;
        /// Nothing is published when the decision is withheld or the state did not change; a change too
        /// large for one event is published as several, in order, each naming some of its cells.
        /// `resync_required`: the state drops a coverage, avatar or environment, which no event
        /// can express; the caller rebases and tells subscribers to start over.
        auto publish( publish_request request ) -> std::expected<std::vector<public_event>, error>;
        /// Publishes the state a patch over the current one stands for, with the event, errors and
        /// effects `publish` of that state would give, at a cost of what the patch names and changes.
        auto publish( patch_request request ) -> std::expected<std::vector<public_event>, error>;
        /// A transient event: consumes a sequence, never a revision. resource_limit when it cannot fit a frame.
        auto publish_message( message_request request ) -> std::expected<public_event, error>;
        /// A transient animation fact: consumes a sequence, never a revision.
        auto publish_presentation( presentation_request request ) -> std::expected<public_event, error>;
        /// Engine-boundary recovery when an event cannot be encoded: adopt `next` and advance the
        /// revision only. Subscribers must be told `resync` and take a fresh snapshot.
        auto rebase( state_value next ) -> std::expected<void, error>;
    private:
        explicit event_stream( snapshot initial );
        /// The events of a delta, with their sequences and revisions taken; none when the delta is empty, and
        /// more than one when it does not fit one. The caller then adopts the state the delta leads to.
        auto issue( changes delta, std::optional<std::string> command,
                    std::optional<counter> cause ) -> std::expected<std::vector<public_event>, error>;
        snapshot current_;
};

/// Transactional reconstruction. Epoch, gap, duplicate, revision or changes violations leave the
/// receiver unchanged; an invalid suffix applies no prefix.
auto apply_event( snapshot &receiver, const std::string &epoch, const public_event &event )
-> std::expected<void, error>;
auto apply_batch( snapshot &receiver, const event_batch &batch ) -> std::expected<void, error>;

/// `type` for a published change set, e.g. `cells.seen`.
auto classify( const changes &delta ) -> std::string;

/// What the avatar knows inside a box of one level, as of `at`.
struct cells_page {
    clock_point at = {};
    std::vector<cell> cells = {};
    /// Squares of the box inside the loaded coverage that the avatar knows nothing about.
    std::vector<position> forgotten = {};
};
/// Passive read of `current` for `bn.world.cells`. `stale_epoch` for another epoch,
/// `validation_failed` for a box that is inverted, spans levels or dimensions, and
/// `resource_limit` for more than cells_per_query squares or a page too large to send.
auto read_cells( const cells_request &request, const snapshot &current )
-> std::expected<cells_page, error>;
auto serialize_cells( const cells_page &page ) -> std::string;

/// Params of `bn.events`, with the `stack` of each boundary an event carries.
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
