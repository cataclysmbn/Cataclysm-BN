#pragma once

#include "animation.h"
#include "client_animation.h"

#include <chrono>
#include <cstdint>
#include <memory>
#include <variant>

namespace presentation
{

/// One presentation fact as the game emits it, whatever displays it.
using fact = std::variant<game_client::bullet_animation_options, draw_bullet_trajectories_options,
      draw_sprite_line_options, game_client::hit_mon_animation_options,
      game_client::hit_player_animation_options, game_client::explosion_animation_options,
      draw_custom_explosion_options, game_client::cone_aoe_animation_options>;

/// Draws and paces what a native display shows. Tiles and curses register one; a headless process
/// has none, so nothing it emits redraws or sleeps.
class sink
{
    public:
        virtual ~sink() = default;
        /// `ordinal` numbers the facts in the order the game caused them.
        virtual auto present( const fact &shown, std::uint64_t ordinal ) -> void = 0;
        /// Holds one animation step on screen.
        virtual auto pace( const game_client::animation_progress_options &options ) -> void = 0;
        /// Shows what one logical time step of an explosion changed.
        virtual auto redraw() -> void = 0;
        /// Keeps the screen as it is until the next logical time step of an explosion is due.
        virtual auto hold( std::chrono::milliseconds duration ) -> void = 0;
};

/// Registers `replacement` and returns the sink it replaces.
auto set_sink( std::unique_ptr<sink> replacement ) -> std::unique_ptr<sink>;
/// The sink a native display registered; none in a headless process.
auto registered() -> sink *; // *NOPAD*
/// Draws with the registered animation service, else in curses.
auto install_native_sink() -> void;
/// Tells a listening client, then the sink.
auto emit( const fact &emitted ) -> void;

} // namespace presentation
