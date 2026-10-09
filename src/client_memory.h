#pragma once

#include "client_snapshot.h"
#include "input.h"

#include <functional>

class nc_color;
struct RGBColor;
namespace catacurses
{
class window;
}

namespace game_client::memory
{

/// A callback invoked when the game needs the next logical input event.
using input_provider = std::function < auto( int timeout_ms )->input_event >;

/// Initializes the shared cell compositor and curses-compatible virtual screens.
auto initialize( int width, int height ) -> void;
auto shutdown() -> void;

/// Installs an input source for a client which owns a composed terminal screen.
auto set_input_provider( input_provider provider ) -> void;

/// Installs the callback invoked after the composed screen is flushed.
auto set_present_callback( std::function < auto( const screen_snapshot & )->void > callback ) ->
void;

/// Returns the most recent complete composed screen.
auto snapshot() -> screen_snapshot;

/// Returns the current logical terminal size in cells.
auto screen_size() -> point;

/// Changes the logical terminal size, preserving as much existing content as possible.
auto resize( int width, int height ) -> void;
auto draw_window( const catacurses::window &window ) -> void;
auto clear_window( const catacurses::window &window ) -> void;
auto set_cursor( int visibility ) -> void;
auto set_timeout( int timeout ) -> void;
auto present() -> void;
auto read_input() -> input_event;
auto color_to_rgb( const nc_color &color ) -> RGBColor;

} // namespace game_client::memory
