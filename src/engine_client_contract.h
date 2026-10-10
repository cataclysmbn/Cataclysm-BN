#pragma once

#include "client_command.h"
#include "client_interaction.h"
#include "engine_client_state.h"

#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace engine_client
{

inline constexpr auto contract_version = "1.0";
inline constexpr auto maximum_rows = std::size_t {200};
inline constexpr auto maximum_inline_bytes = std::size_t {262144};
inline constexpr auto maximum_frame_bytes = std::size_t {1048576};
inline constexpr auto cells_per_part = std::size_t {512};
inline constexpr auto cells_per_query = std::size_t {4096};
inline constexpr auto maximum_safe_integer = std::uint64_t {9007199254740991};
inline constexpr auto maximum_id_bytes = std::size_t {256};
using counter = std::uint64_t;

/// Stable machine-readable errors. Details from native resolvers are intentionally not exported.
enum class error {
    negotiation_failed, not_ready, stale_epoch, stale_revision, stale_boundary,
    stale_interaction_schema, validation_failed, command_busy, unknown_command,
    invalid_lifecycle, resync_required, resource_limit,
};
auto error_name( error value ) -> std::string;

/// Invoke once for each new authoritative session/world/process, never on a read.
auto new_session_epoch() -> std::expected<std::string, error>;

/// `(epoch, sequence, revision)`: the state includes every event up to `sequence`.
struct clock_point {
    std::string epoch = {};
    counter sequence = 0;
    counter revision = 0;
    auto operator<=>( const clock_point & ) const = default; // *NOPAD*
};

struct readiness {
    std::string phase = "starting";
    bool game_ready = false;
    bool accepts_interaction_commands = false;
    bool accepts_registered_actions = false;
};
struct action {
    std::string id = {};
    std::string name = {};
    /// Portable names of the single keys that run this action in the active input context.
    std::vector<std::string> keys = {};
};
/// Where the reality bubble sits in absolute coordinates. Native interaction positions are
/// bubble-relative; they become absolute only on the wire.
struct bubble_frame {
    std::string dim = {};
    int x = 0;
    int y = 0;
};
auto current_bubble_frame() -> bubble_frame;
/// Absolute square to bubble coordinates; nullopt in another dimension or when the result does
/// not fit the native int range.
auto to_bubble( const position &target, const bubble_frame &frame )
-> std::optional<game_client::interaction_position>;

/// What the player may do at one native input boundary. The numeric native input_id stays internal.
struct boundary_state {
    std::string id = {};
    readiness ready = {};
    std::vector<action> actions = {};
    std::optional<game_client::interaction_snapshot> interaction = std::nullopt;
    bubble_frame frame = {};
};
struct capture_options {
    std::string epoch = {};
    readiness ready = {};
};
/// Only call at a stable native input boundary. Reads choices `[0, 200)`, fewer when the
/// value would exceed maximum_inline_bytes; `choice_total` always states the full count.
/// This does not publish anything.
auto capture_boundary( const capture_options &options ) -> std::expected<boundary_state, error>;
auto validate_boundary( const boundary_state &state ) -> std::expected<void, error>;
auto same_boundary( const boundary_state &left, const boundary_state &right ) -> bool;

struct choices_request {
    std::string epoch = {};
    std::string boundary_id = {};
    std::size_t offset = 0;
    std::size_t limit = 100;
};
struct choices_page {
    std::string boundary_id = {};
    std::size_t total = 0;
    std::vector<game_client::interaction_choice> choices = {};
};
/// Passive page read of the live interaction at `epoch`'s current boundary.
auto read_choices( const choices_request &request, const std::string &epoch )
-> std::expected<choices_page, error>;

struct registered_action {
    std::string id = {};
};
/// Native semantic fields retain their exact semantics. `target` is absolute; the native
/// bounds and range checks apply after it is mapped into the bubble.
struct semantic_operation {
    game_client::interaction_command command;
    std::optional<position> target = std::nullopt;
};
/// A left click on an absolute map square, handled as in Tiles and curses: the first click plans
/// the native route (`world_state::route`), a second click on its end starts native auto-move.
struct travel_operation {
    position target;
};
/// The native left-click input that selects `operation.target`; nullopt when another dimension or
/// level, or a square outside the terrain window.
auto travel_click( const travel_operation &operation ) -> std::optional<game_client::input_command>;
struct expectation {
    counter revision = 0;
    std::string boundary_id = {};
    /// Null exactly when the boundary has no interaction.
    std::optional<std::string> schema_id = std::nullopt;
};
struct command_request {
    std::string epoch = {};
    expectation expect = {};
    std::variant<semantic_operation, registered_action, travel_operation> operation;
};
enum class command_stage { received, validated, executing, rejected, completed, interrupted };
auto stage_name( command_stage stage ) -> std::string;
struct command_result {
    std::string epoch = {};
    std::string command_id = {};
    command_stage stage = command_stage::received;
    std::optional<error> failure = std::nullopt;
    /// Endpoint of the boundary that completed the command.
    std::optional<clock_point> at = std::nullopt;
};

struct command_permissions {
    bool accepts_interaction_commands = false;
    bool accepts_registered_actions = false;
};
/// Engine-session-owned policy, read on the game thread at validation time.
class command_authority
{
    public:
        virtual ~command_authority() = default;
        virtual auto current_permissions() const -> command_permissions = 0;
};

/// One outstanding command and one replaceable terminal result, not a history map.
class command_lifecycle
{
    public:
        command_lifecycle( std::string epoch, const command_authority &authority );
        auto submit( command_request request ) -> std::expected<command_result, error>;
        /// Pure native resolution, not callback invocation or execution.
        auto validate( const clock_point &at, const boundary_state &current, point screen_size )
        -> std::expected<input_event, error>;
        /// Call only when delivering the resolved input to the native widget.
        auto execution_started() -> std::expected<void, error>;
        /// Next *distinct* native input boundary; this is not long-activity completion.
        auto complete_at_boundary( const clock_point &at, const boundary_state &current )
        -> std::expected<void, error>;
        /// The reason is the result's `failure`; `stale_epoch` for a replaced world, `not_ready` for a closing connection, `validation_failed` for input the game refused.
        auto interrupt( error reason ) -> std::expected<void, error>;
        auto result( const std::string &command_id ) const -> std::expected<command_result, error>;
    private:
        std::string epoch_ = {};
        const command_authority &authority_;
        counter next_command_ = 0;
        std::optional<command_request> request_ = std::nullopt;
        std::optional<command_result> result_ = std::nullopt;
};

auto serialize_clock( const clock_point &at ) -> std::string;
auto serialize_boundary( const boundary_state &state ) -> std::string;
auto serialize_choices( const choices_page &page ) -> std::string;
/// Params of `bn.command` and result of `bn.command.result`.
auto serialize_command_result( const command_result &value ) -> std::string;

} // namespace engine_client
