#pragma once

#include "client_command.h"
#include "client_interaction.h"
#include "engine_client_mode.h"
#include "engine_client_state.h"
#include "engine_client_world.h"
#include "tile_parts.h"

#include <cstdint>
#include <deque>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace engine_client
{

/// The one version a hello may negotiate; it names the schema and the boundaries carry the `stack`.
inline constexpr auto contract_version = "0.2";
/// Screen-scoped invokes a controller may have waiting behind the running command.
inline constexpr auto maximum_queued_commands = std::size_t {8};
inline constexpr auto maximum_rows = std::size_t {200};
inline constexpr auto maximum_inline_bytes = std::size_t {262144};
/// The most bytes of rows one list puts on the wire, in the boundary and in a page of `bn.interaction.choices`;
/// `total` states the rest, which the client pages. The first row always goes.
inline constexpr auto maximum_list_bytes = maximum_inline_bytes / 8;
/// Delivery credits a client may hold, each paying for one pushed `bn.events` or `bn.command` frame.
inline constexpr auto maximum_credits = std::size_t {256};
/// The most frames per second a client may ask activity publication for; a display refreshes no faster.
inline constexpr auto maximum_frame_rate = std::size_t {240};
/// What the engine keeps for a client that holds no credits: past either bound it drops all of it
/// and sends `bn.resync` instead.
inline constexpr auto maximum_held_frames = std::size_t {256};
inline constexpr auto maximum_held_bytes = std::size_t {1048576};
inline constexpr auto maximum_frame_bytes = std::size_t {1048576};
inline constexpr auto cells_per_part = std::size_t {512};
/// The newest message log lines a snapshot carries.
inline constexpr auto maximum_log_lines = std::size_t {100};
inline constexpr auto cells_per_query = std::size_t {4096};
inline constexpr auto maximum_safe_integer = std::uint64_t {9007199254740991};
inline constexpr auto maximum_id_bytes = std::size_t {256};
using counter = std::uint64_t;

/// Stable machine-readable errors. Details from native resolvers are intentionally not exported.
enum class error {
    negotiation_failed, not_ready, stale_epoch, stale_revision, stale_boundary,
    stale_interaction_schema, validation_failed, command_busy, unknown_command,
    invalid_lifecycle, resync_required, resource_limit, read_only,
    /// 0.2: an invoke's arguments do not match its affordance's shape.
    invalid_arguments,
    /// 0.2: the boundary offers no affordance with the invoked id.
    unknown_affordance,
    /// 0.2: a command expecting a screen found another on top of the stack.
    stale_screen,
    /// 0.2: a hello asks for a language the engine has no catalogue for.
    unavailable_locale,
};
auto error_name( error value ) -> std::string;
/// The error in words for a player, localized.
auto error_text( error value ) -> std::string;

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
    auto operator<=>( const action & ) const = default; // *NOPAD*
};
/// Where the reality bubble sits in absolute coordinates. Native interaction positions are
/// bubble-relative; they become absolute only on the wire.
struct bubble_frame {
    std::string dim = {};
    int x = 0;
    int y = 0;
    /// The overmap is open: native squares are overmap terrain squares, already absolute.
    bool omt = false;
};
auto current_bubble_frame() -> bubble_frame;
/// Absolute square to bubble coordinates; nullopt in another dimension or when the result does
/// not fit the native int range.
auto to_bubble( const position &target, const bubble_frame &frame )
-> std::optional<game_client::interaction_position>;

struct screen_view;
/// What the player may do at one native input boundary. The numeric native input_id stays internal.
struct boundary_state {
    std::string id = {};
    /// The session epoch the boundary belongs to; names the map screen.
    std::string epoch = {};
    readiness ready = {};
    std::vector<action> actions = {};
    std::optional<game_client::interaction_snapshot> interaction = std::nullopt;
    bubble_frame frame = {};
    /// The open screens as `capture_boundary` found them, so that what is published stays what the
    /// client saw. Null for a state not captured: serialization then reads the live screens.
    std::shared_ptr<const std::vector<screen_view>> stack = nullptr;
};
struct capture_options {
    std::string epoch = {};
    readiness ready = {};
};
/// The portable names of the single keyboard keys among `bindings`.
auto key_names( const std::vector<input_event> &bindings ) -> std::vector<std::string>;
/// Only call at a stable native input boundary. Reads choices `[0, 200)`, fewer when the
/// value would exceed maximum_inline_bytes; `choice_total` always states the full count.
/// This does not publish anything.
auto capture_boundary( const capture_options &options ) -> std::expected<boundary_state, error>;
/// The id the boundary has now, whatever was published last.
auto live_boundary_of( const std::string &epoch ) -> std::string;
auto validate_boundary( const boundary_state &state ) -> std::expected<void, error>;
auto same_boundary( const boundary_state &left, const boundary_state &right ) -> bool;

struct choices_request {
    std::string epoch = {};
    std::string boundary_id = {};
    /// A `list` of the top screen whose rows are read (0.2); absent: the 0.1 choices of the interaction.
    std::optional<std::string> component_id = std::nullopt;
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

struct describe_request {
    std::string epoch = {};
    std::string boundary_id = {};
    position target;
};
/// Something the avatar can do with a square now. The id is the native action's, or `travel`, `attack`
/// or `talk`; the label and keys are the native action's own where it has one.
struct tile_action {
    std::string id = {};
    std::string label = {};
    std::vector<std::string> keys = {};
};
/// What the native mouse view prints for a square, as the facts it prints, and what the avatar can do there.
struct tile_description {
    std::vector<tile_part> parts = {};
    std::vector<tile_action> actions = {};
};
/// The actions of `tile_action` the avatar can take on `at` now, in the order a menu lists them.
auto tile_actions( const tripoint_bub_ms &at ) -> std::vector<tile_action>;
/// Passive read of the hovered square's native description; nothing is selected or planned.
/// Rejected with `validation_failed` outside the terrain window or on another level.
auto describe_tile( const describe_request &request, const std::string &epoch )
-> std::expected<tile_description, error>;

struct cells_request {
    std::string epoch = {};
    position min;
    position max;
};

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
    /// `left` is Tiles' SELECT; `right` is SEC_SELECT, which examines, closes, picks up or fires.
    std::string button = "left";
};
/// The native input that travels to `operation.target`: its click, or for a left click on a closed door
/// beside the avatar the move into it, which opens it. Nullopt when another dimension or level, or a
/// square outside the terrain window.
auto travel_click( const travel_operation &operation ) -> std::optional<game_client::input_command>;
/// Picks up `count` of a row of `avatar.ground`, or drops `count` of a row of `avatar.inventory`, in the row's native units
/// (charges of a charges item, else items), as the native pickup and drop menus do once the rows are marked and confirmed.
struct item_operation {
    bool drop = false;
    std::string item_id = {};
    std::uint64_t count = 0;
};
/// The input that wakes the native read for an item or tile operation; the session hands the game
/// `item_operation_event` or `tile_operation_event` in its place.
auto item_operation_input() -> game_client::input_command;
/// The input the map view reads for an item operation and carries out (`game::handle_action`); replay
/// records and plays it back like any other input.
auto item_operation_event( const item_operation &operation ) -> input_event;
/// The item operation `event` carries; nullopt for any other input.
auto item_operation_of( const input_event &event ) -> std::optional<item_operation>;
/// The row `operation` names, with the items it stands for; nullopt when no such row holds that much.
auto item_operation_stack( const item_operation &operation ) -> std::optional<world::item_stack>;
/// Starts the native pickup or drop activity for `operation`; false when its row is gone or holds less.
auto run_item_operation( const item_operation &operation ) -> bool;
/// Takes one of the `tile_actions` of an absolute map square, as the native action on that square does.
struct tile_operation {
    std::string action_id = {};
    position target;
};
/// The input the map view reads for a tile operation and carries out (`game::handle_action`); replay records
/// and plays it back like any other input. Nullopt when the square is outside the terrain window.
auto tile_operation_event( const tile_operation &operation ) -> std::optional<input_event>;
/// The tile operation `event` carries: its action id and the square in the bubble.
struct tile_command {
    std::string action_id = {};
    tripoint_bub_ms at;
};
auto tile_operation_of( const input_event &event ) -> std::optional<tile_command>;
struct invoke_row {
    std::string id = {};
    std::optional<std::uint64_t> count = std::nullopt;
};
/// The arguments of a 0.2 invoke; `pos` is absolute.
struct invoke_args {
    std::optional<std::string> row = std::nullopt;
    std::optional<std::vector<invoke_row>> rows = std::nullopt;
    std::optional<std::int64_t> count = std::nullopt;
    std::optional<std::string> text = std::nullopt;
    std::optional<position> pos = std::nullopt;
    std::optional<std::string> pane = std::nullopt;
};
/// The one 0.2 operation: an affordance of the top screen, by id. It also stands for an affordance the
/// screen runs itself, which reaches the screen's loop as a recorded `invoke` input.
struct invoke_operation {
    std::string affordance_id = {};
    invoke_args args = {};
};
/// What a command is conditional on. A 0.1 operation expects the boundary, its revision and the schema of
/// its interaction; an invoke expects either `screen_id` alone or the boundary and its revision, as the
/// scope of its affordance says (`scope_of`), and names no schema.
struct expectation {
    counter revision = 0;
    std::string boundary_id = {};
    /// Null exactly when the boundary has no interaction.
    std::optional<std::string> schema_id = std::nullopt;
    /// Set: the command expects this screen on top of the stack, and `revision` and `boundary_id` are unused.
    std::optional<std::string> screen_id = std::nullopt;
};
/// Which expectation an invoke of an affordance carries; the engine assigns it by the arguments.
enum class expect_scope { screen, boundary };
auto scope_name( expect_scope scope ) -> std::string;
using command_operation = std::variant<semantic_operation, registered_action, travel_operation,
      item_operation, tile_operation, mode_operation, invoke_operation>;
struct command_request {
    std::string epoch = {};
    expectation expect = {};
    command_operation operation;
};
/// The native operation an invoke stands for at `state`: `unknown_affordance` when the top screen offers
/// no such affordance, `invalid_arguments` when `args` does not match its shape or names a row the
/// component does not hold, `not_ready` when readiness refuses it, `validation_failed` when it is
/// disabled. Pure; validation of the result against the live widget is the lifecycle's. An affordance its
/// screen runs itself resolves to the invoke.
auto resolve_invoke( const boundary_state &state, const invoke_operation &invoke )
-> std::expected<command_operation, error>;
enum class command_stage { received, validated, executing, rejected, completed, interrupted };
auto stage_name( command_stage stage ) -> std::string;
/// The player's words for `stage`, localized.
auto stage_text( command_stage stage ) -> std::string;
struct command_result {
    std::string epoch = {};
    std::string command_id = {};
    command_stage stage = command_stage::received;
    std::optional<error> failure = std::nullopt;
    /// Why, localized, where the engine knows more than the error kind; empty otherwise.
    std::string message = {};
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

/// A waiting command that has become the running one.
struct promoted_command {
    command_result result;
    command_request request;
};

/// One outstanding command and one replaceable terminal result, not a history map; behind it wait at most
/// `maximum_queued_commands` screen-scoped invokes, each validated only when it runs.
class command_lifecycle
{
    public:
        command_lifecycle( std::string epoch, const command_authority &authority );
        auto submit( command_request request ) -> std::expected<command_result, error>;
        /// A command is running and has not ended.
        auto busy() const -> bool;
        /// Queues a screen-scoped invoke behind the running command, with the receipt it will keep.
        /// `command_busy` for any other request or when the queue is full, `invalid_lifecycle` when no
        /// command is running (it is submitted instead).
        auto enqueue( command_request request ) -> std::expected<command_result, error>;
        /// The oldest waiting command takes over once the running one has ended; nullopt otherwise.
        auto promote() -> std::optional<promoted_command>;
        /// Ends every waiting command as interrupted by `reason` and returns what became of each.
        auto drain( error reason ) -> std::vector<command_result>;
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
        /// The boundary the command was validated at; it completes at the next distinct one.
        std::string boundary_ = {};
        std::deque<promoted_command> queue_ = {};
        /// The receipt of a request that passes the shape and limit checks, with its id taken.
        auto receive( const command_request &request ) -> std::expected<command_result, error>;
};

auto serialize_clock( const clock_point &at ) -> std::string;
auto serialize_boundary( const boundary_state &state ) -> std::string;
auto serialize_choices( const choices_page &page ) -> std::string;
/// The `overmap` value of the 0.2 schema: the squares, with the positions absolute.
auto serialize_overmap( const game_client::interaction_overmap &squares ) -> std::string;
/// Params of `bn.command` and result of `bn.command.result`.
auto serialize_command_result( const command_result &value ) -> std::string;

} // namespace engine_client
