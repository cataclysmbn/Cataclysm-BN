#pragma once

#include "client_command.h"
#include "client_interaction.h"

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
inline constexpr auto maximum_inline_events = std::size_t {8};
inline constexpr auto maximum_inline_bytes = std::size_t {262144};
inline constexpr auto maximum_safe_integer = std::uint64_t {9007199254740991};
inline constexpr auto maximum_id_bytes = std::size_t {256};
using counter = std::uint64_t;

/// Stable machine-readable errors. Details from native resolvers are intentionally not exported.
enum class error {
    negotiation_failed, unsupported_capability, not_ready, stale_epoch, stale_revision,
    stale_boundary, stale_interaction_schema, validation_failed, command_busy,
    unknown_command, invalid_lifecycle, resync_required, resource_limit,
};
auto error_name( error value ) -> std::string;

struct negotiation_request {
    std::vector<std::string> supported_versions = {};
    std::vector<std::string> required_capabilities = {};
    std::vector<std::string> optional_capabilities = {};
};
struct negotiated_contract {
    std::vector<std::string> capabilities = {};
};
auto negotiate( const negotiation_request &request ) -> std::expected<negotiated_contract, error>;
/// Invoke once for each new authoritative session/world/process, never on a read.
auto new_session_epoch() -> std::expected<std::string, error>;

/// Published information only; validation reads the bound live engine authority instead.
struct readiness {
    std::string phase = "starting";
    bool game_ready = false;
    bool accepts_interaction_commands = false;
    bool accepts_registered_actions = false;
};
struct action {
    std::string id = {};
    std::string name = {};
};
/// Structural identity of a requested bounded view, independent of authority clocks/epochs.
struct projection {
    std::size_t offset = 0;
    std::size_t limit = 100;
    auto operator<=>( const projection & ) const = default; // *NOPAD*
};
/// Reuses the owned native interaction model. Its numeric input_id is internal, not a wire ID.
struct state_value {
    readiness ready = {};
    std::string input_boundary_id = {};
    std::vector<action> actions = {};
    std::optional<game_client::interaction_snapshot> interaction = std::nullopt;
    projection view = {};
};
struct snapshot {
    std::string session_epoch = {};
    counter state_revision = 0;
    counter through_public_sequence = 0;
    state_value state;
};
struct capture_options {
    std::string session_epoch = {};
    readiness ready = {};
    game_client::interaction_page page = {};
};
/// Only call at a stable native input boundary, with a P1-approved pure provider.
/// This does not commit a revision or perform visibility acquisition.
auto capture_state( const capture_options &options ) -> std::expected<state_value, error>;
auto validate_state( const state_value &state ) -> std::expected<void, error>;
auto same_state( const state_value &left, const state_value &right ) -> bool;
/// Compare boundary-wide metadata, excluding the requested page and its choice rows.
/// Engine captures at one stable boundary supply the rows; this is not an untrusted parser.
auto same_boundary_state( const state_value &left, const state_value &right ) -> bool;

/// A coordinate cannot be interpreted without its explicit space/frame/dimension.
struct coordinate {
    std::string space = {};
    std::string frame_id = {};
    std::string dimension_id = {};
    game_client::interaction_position position;
};
struct registered_action {
    std::string id = {};
};
/// Native semantic fields retain their exact semantics. input_id is overwritten at live validation.
struct semantic_operation {
    game_client::interaction_command command;
    std::optional<coordinate> target = std::nullopt;
};
struct command_request {
    std::string session_epoch = {};
    counter state_revision = 0;
    std::string input_boundary_id = {};
    std::string interaction_schema_id = {};
    std::variant<semantic_operation, registered_action> operation;
};
enum class command_stage { received, validated, executing, rejected, completed, interrupted };
struct completion {
    counter state_revision = 0;
    counter through_public_sequence = 0;
    bool resync_required = false;
};
struct command_result {
    std::string session_epoch = {};
    std::string command_id = {};
    command_stage stage = command_stage::received;
    bool validation_succeeded = false;
    bool execution_started = false;
    std::optional<error> failure = std::nullopt;
    std::optional<completion> completed = std::nullopt;
};

struct command_permissions {
    bool accepts_interaction_commands = false;
    bool accepts_registered_actions = false;
};
/// Engine-session-owned policy, read on the game thread at validation time.
/// Must outlive the lifecycle. Implementations read current policy, never cached wire DTOs,
/// never execute gameplay callbacks, and are not supplied by I/O or individual requests.
class command_authority
{
    public:
        virtual ~command_authority() = default;
        virtual auto current_permissions() const -> command_permissions = 0;
};

/// One outstanding command and one replaceable terminal result, not a history map.
/// The adapter must gate negotiated capabilities before submit and never deliver twice.
class command_lifecycle
{
    public:
        command_lifecycle( std::string session_epoch, const command_authority &authority );
        auto submit( command_request request ) -> std::expected<command_result, error>;
        /// Pure native resolution, not callback invocation or execution.
        auto validate( const snapshot &current, point screen_size ) -> std::expected<input_event, error>;
        /// Call only when delivering the resolved input to the native widget.
        auto execution_started() -> std::expected<void, error>;
        /// Next *distinct* native input boundary; this is not long-activity completion.
        auto complete_at_boundary( const snapshot &current, bool resync_required = false )
        -> std::expected<void, error>;
        auto interrupt() -> std::expected<void, error>;
        auto result( const std::string &command_id ) const -> std::expected<command_result, error>;
    private:
        std::string epoch_ = {};
        const command_authority &authority_;
        counter next_command_ = 0;
        std::optional<command_request> request_ = std::nullopt;
        std::optional<command_result> result_ = std::nullopt;
};

auto serialize_state( const state_value &state ) -> std::string;
auto serialize_snapshot( const snapshot &value ) -> std::string;
auto serialize_negotiation( const negotiated_contract &value,
                            const std::string &epoch ) -> std::string;
auto serialize_result( const command_result &value ) -> std::string;

} // namespace engine_client
