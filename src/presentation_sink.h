#pragma once

#include "animation.h"
#include "client_animation.h"

#include <cstdint>
#include <memory>
#include <variant>

namespace presentation
{

/// One presentation fact as the game emits it, whatever displays it.
using fact = std::variant<game_client::bullet_animation_options, draw_bullet_trajectories_options,
      draw_sprite_line_options, game_client::hit_mon_animation_options,
      game_client::hit_player_animation_options>;

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
};

auto set_sink( std::unique_ptr<sink> replacement ) -> void;
/// Draws with the registered animation service, else in curses.
auto install_native_sink() -> void;
/// Tells a listening client, then the sink.
auto emit( const fact &emitted ) -> void;

} // namespace presentation
