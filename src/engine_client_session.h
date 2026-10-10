#pragma once

#include "engine_client_event.h"
#include "engine_client_wire.h"
#include "message_feed.h"

#include <memory>
#include <variant>

namespace engine_client
{

/// Something the connection must push at its next flush, in publication order.
struct resync_notice {
    std::string epoch = {};
    std::string reason = {};
    counter lost_after = 0;
};
struct event_push {
    std::string epoch = {};
    public_event event;
};
using push_item = std::variant<event_push, command_result, resync_notice>;

/// World capture hook. Called on the game thread at every input boundary and returns
/// everything the avatar currently knows (coverage, cells, entities, avatar, environment).
/// The session diffs it against the previous capture; the hook never produces events and must
/// not change game state, RNG or memory. Unset means no world is known (menus).
using world_capture_fn = auto( * )() -> world_state;

/// Game-thread authority shared by connections. Reads and hello never reset its clock.
class session final : public command_authority
{
    public:
        session();
        auto epoch() const -> const std::string &; // *NOPAD*
        auto current_permissions() const -> command_permissions override;
        auto set_phase( std::string phase ) -> void;
        auto set_world_capture( world_capture_fn capture ) -> void;
        /// World replacement interrupts the old receipt, queues `resync` and installs a new epoch.
        auto replace_world() -> void;
        /// Capture the live boundary and publish its change as one event.
        auto publish_boundary() -> std::expected<void, error>;
        /// Atomic with respect to publication: the game thread is the only writer.
        auto current() const -> std::expected<snapshot, error>;
        auto at() const -> std::optional<clock_point>;
        auto submit( command_request request ) -> std::expected<receipt, error>;
        auto prepare_input( point screen_size ) -> std::expected<game_client::input_command, error>;
        /// Return the core-validated event at the actual backend delivery seam.
        auto delivered( input_event fallback ) -> input_event;
        auto interrupt( error reason ) -> void;
        auto has_received() const -> bool;
        auto result( const result_request &request ) const -> std::expected<command_result, error>;
        /// Everything published since the last call; the connection decides what to send.
        auto take_push() -> std::vector<push_item>;
    private:
        std::string epoch_;
        std::string phase_ = "starting";
        world_capture_fn world_capture_ = nullptr;
        std::unique_ptr<command_lifecycle> lifecycle_;
        std::optional<command_result> active_;
        std::optional<command_result> retired_;
        std::optional<command_request> input_;
        std::optional<input_event> validated_input_;
        std::optional<event_stream> stream_;
        std::vector<push_item> push_;
        /// What the stream has already announced of the native message log.
        Messages::feed_cursor message_cursor_;
        auto capture() const -> std::expected<state_value, error>;
        auto restart_epoch( std::string_view reason ) -> void;
        auto refresh_result() -> void;
        auto publish_messages( const std::optional<std::string> &command ) -> void;
};

auto process_session() -> session &; // *NOPAD*
/// Build string and loaded mod IDs for `bn.hello`.
auto describe_engine() -> engine_info;

} // namespace engine_client
