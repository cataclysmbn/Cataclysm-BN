#pragma once

#include "engine_client_event.h"
#include "engine_client_wire.h"

#include <memory>

namespace engine_client
{

/// Game-thread authority shared by connections. Reads and negotiation never reset its clock.
class session final : public command_authority
{
    public:
        session();
        auto epoch() const -> const std::string &; // *NOPAD*
        auto current_permissions() const -> command_permissions override;
        auto set_phase( std::string phase ) -> void;
        /// World replacement interrupts the old receipt before installing a new epoch.
        auto replace_world() -> void;
        auto publish_boundary() -> std::expected<void, error>;
        auto read_snapshot( projection page ) const -> std::expected<snapshot, error>;
        auto submit( command_request request ) -> std::expected<receipt, error>;
        auto prepare_input( point screen_size ) -> std::expected<game_client::input_command, error>;
        /// Return the core-validated event at the actual backend delivery seam.
        auto delivered( input_event fallback ) -> input_event;
        auto interrupt() -> void;
        auto has_received() const -> bool;
        auto result( const result_request &request ) const -> std::expected<command_result, error>;
        auto latest_event() const -> const std::optional<public_event> &; // *NOPAD*
    private:
        std::string epoch_;
        std::string phase_ = "starting";
        std::unique_ptr<command_lifecycle> lifecycle_;
        std::optional<command_result> active_;
        std::optional<command_result> retired_;
        std::optional<command_request> input_;
        std::optional<input_event> validated_input_;
        std::optional<event_stream> stream_;
        std::optional<public_event> event_;
        auto capture( projection page ) const -> std::expected<state_value, error>;
        auto refresh_result() -> void;
};

auto process_session() -> session &; // *NOPAD*

} // namespace engine_client
