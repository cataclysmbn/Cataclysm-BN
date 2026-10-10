#pragma once

#include "coordinates.h"
#include "input.h"
#include "point.h"

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

namespace catacurses
{
class window;
}

namespace game_client
{

enum class client_kind : int {
    tiles,
    curses,
    mcp,
    imgui,
};

struct client_capabilities {
    bool tiles = false;
    bool mouse = false;
    bool gamepad = false;
    bool requires_display = false;
};

/// Runtime device boundary shared by the game loop and every display client.
class backend
{
    public:
        virtual ~backend() = default;
        virtual auto native_window_handle() const -> void* { return nullptr; }

        /// Performs client setup that must happen after selection and before game data loads.
        virtual auto prepare() -> void {}
        virtual auto initialize() -> void = 0;
        virtual auto shutdown() -> void = 0;
        virtual auto present() -> void = 0;
        virtual auto draw_window( const catacurses::window &window ) -> void = 0;
        virtual auto clear_window( const catacurses::window &window ) -> void = 0;
        virtual auto set_cursor( int visibility ) -> void = 0;
        virtual auto set_timeout( int timeout_ms ) -> void = 0;
        virtual auto read_input( int timeout_ms ) -> input_event = 0;
        virtual auto pump_events() -> void = 0;
        /// Called before each step of a native auto-move, which reads no input in between.
        virtual auto step_boundary() -> void {}
        virtual auto resize( point cell_size ) -> void = 0;
        virtual auto projected_size() const -> point = 0;
        virtual auto terminal_size() const -> point { return projected_size(); }
        virtual auto project_input_coordinates(
            point /*coordinate*/, const catacurses::window & /*capture_window*/ ) const
        -> std::optional<tripoint_bub_ms> {
            return std::nullopt;
        }
        virtual auto scaling_factor() const -> int { return 1; }
        virtual auto capabilities() const -> client_capabilities = 0;
};

using backend_ptr = std::unique_ptr<backend>;
using backend_factory = std::function < auto()->backend_ptr >;

/// Registers a runtime client implementation before the client is selected.
auto register_backend( client_kind kind, backend_factory factory ) -> void;

/// Registers every backend compiled into this executable.
auto register_builtin_backends() -> void;

/// Chooses the preferred backend when --client is omitted.
auto default_client_kind() -> client_kind;

/// Selects the process-wide client backend. Selection is valid once per process.
auto set_active_backend( backend_ptr client ) -> void;

/// Returns the selected client backend.
auto active_backend() -> backend&; // *NOPAD*
auto backend_selected() -> bool;

/// Creates a registered backend for a command-line client kind.
auto create_backend( client_kind kind ) -> backend_ptr;

/// Converts a command-line client name to its runtime kind.
auto parse_client_kind( const std::string &name ) -> client_kind;

auto make_tiles_backend() -> backend_ptr;
auto make_curses_backend() -> backend_ptr;
auto make_mcp_backend() -> backend_ptr;
auto make_imgui_backend() -> backend_ptr;

namespace curses
{
auto draw_window_native( const catacurses::window &window ) -> void;
auto clear_window_native( const catacurses::window &window ) -> void;
auto initialize_native() -> void;
auto shutdown_native() -> void;
auto present_native() -> void;
auto read_input_native() -> input_event;
auto pump_events_native() -> void;
auto set_timeout_native( int timeout_ms ) -> void;
auto set_cursor_native( int visibility ) -> void;
auto resize_native( int width, int height ) -> void;
auto terminal_width_native() -> int;
auto terminal_height_native() -> int;
} // namespace curses

namespace mcp
{
auto draw_window_native( const catacurses::window &window ) -> void;
auto clear_window_native( const catacurses::window &window ) -> void;
auto initialize_native() -> void;
auto shutdown_native() -> void;
auto present_native() -> void;
auto read_input_native() -> input_event;
auto pump_events_native() -> void;
auto set_timeout_native( int timeout_ms ) -> void;
auto set_cursor_native( int visibility ) -> void;
} // namespace mcp

namespace tiles
{
auto draw_window_native( const catacurses::window &window ) -> void;
auto clear_window_native( const catacurses::window &window ) -> void;
auto clear_window_area_native( const catacurses::window &window ) -> void;
auto project_input_coordinates( point coordinate, const catacurses::window &capture_window )
-> std::optional<tripoint_bub_ms>;
auto handle_resize_native( int width, int height ) -> bool;
auto resize_native( int width, int height ) -> void;
auto projected_width_native() -> int;
auto projected_height_native() -> int;
auto terminal_width_native() -> int;
auto terminal_height_native() -> int;
auto scaling_factor_native() -> int;
auto initialize_native() -> void;
auto shutdown_native() -> void;
auto present_native() -> void;
auto read_input_native() -> input_event;
auto pump_events_native() -> void;
auto set_timeout_native( int timeout_ms ) -> void;
auto set_cursor_native( int visibility ) -> void;
auto gamepad_available_native() -> bool;
} // namespace tiles

namespace windows
{
auto draw_window_native( const catacurses::window &window ) -> void;
auto clear_window_native( const catacurses::window &window ) -> void;
auto handle_resize_native( int width, int height ) -> bool;
auto projected_width_native() -> int;
auto projected_height_native() -> int;
auto terminal_width_native() -> int;
auto terminal_height_native() -> int;
auto scaling_factor_native() -> int;
auto resize_native( int width, int height ) -> void;
auto initialize_native() -> void;
auto shutdown_native() -> void;
auto present_native() -> void;
auto read_input_native() -> input_event;
auto pump_events_native() -> void;
auto set_timeout_native( int timeout_ms ) -> void;
auto set_cursor_native( int visibility ) -> void;
} // namespace windows

} // namespace game_client
