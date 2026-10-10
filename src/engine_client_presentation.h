#pragma once

#include "animation.h"
#include "coordinates.h"
#include "engine_client_state.h"
#include "enums.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace engine_client
{

struct text_segment {
    std::string text = {};
    /// Native color name of the message type.
    std::string color = {};
    auto operator<=>( const text_segment & ) const = default; // *NOPAD*
};
/// One native animation fact: what the Tiles and curses renderers would draw, before any renderer.
/// Transient: it changes no state.
struct presentation_value {
    /// `projectile.moved`, `explosion.started`, `explosion.blast`, `explosion.shrapnel`, `explosion.ended` or `combat_text.shown`.
    std::string type = {};
    /// Opaque ID of the projectile or explosion; empty for combat text.
    std::string id = {};
    /// The path of a projectile or the squares of one blast ring, in drawing order.
    std::vector<position> cells = {};
    std::optional<position> at = std::nullopt;
    std::optional<look> appearance = std::nullopt;
    std::optional<std::uint64_t> radius = std::nullopt;
    std::string color = {};
    std::vector<text_segment> segments = {};
    /// How long the native animation holds this fact, from the ANIMATION_DELAY option.
    std::uint64_t duration_ms = 0;
    auto operator<=>( const presentation_value & ) const = default; // *NOPAD*
};

} // namespace engine_client

/// Hooks at the renderer-independent animation entry points (src/animation.cpp, src/output.cpp).
/// Nothing is recorded until a session starts collecting.
namespace engine_client::presentation
{

auto collect( bool on ) -> void;
/// Everything recorded since the last call, in the order the game produced it.
auto take() -> std::vector<presentation_value>;

struct bullet_step {
    tripoint_bub_ms at;
    int index = 0;
    char bullet = '*';
    const std::string &custom_sprite;
};
auto record_bullet( const bullet_step &step ) -> void;
auto record_trajectories( const draw_bullet_trajectories_options &options ) -> void;
/// A shot drawn as one line of sprites (the BULLETS_AS_LASERS default), for a single animation frame.
auto record_line( const draw_sprite_line_options &options ) -> void;
/// A timed explosion (ExplosionProcess): `begin_blast` returns the ID the frames and the end carry.
auto begin_blast( const tripoint_bub_ms &at, int radius, bool fiery ) -> std::string;
/// The squares one logical time step blasted and the shrapnel landed on; nothing when both are empty.
auto record_blast_frame( const std::string &id, const std::vector<tripoint_bub_ms> &blast,
                         const std::vector<tripoint_bub_ms> &shrapnel ) -> void;
auto end_blast( const std::string &id ) -> void;
auto record_explosion( const tripoint_bub_ms &at, int radius, const nc_color &color ) -> void;
auto record_custom_explosion( const tripoint_bub_ms &at,
                              const std::map<tripoint_bub_ms, nc_color> &area ) -> void;
auto record_text( point at, const std::string &first, game_message_type first_type,
                  const std::string &second, game_message_type second_type ) -> void;

} // namespace engine_client::presentation
