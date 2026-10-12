#pragma once

#include "animation.h"
#include "coordinates.h"
#include "engine_client_state.h"
#include "enums.h"
#include "type_id.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

class Creature;
class item;

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
    /// `projectile.moved`, `explosion.started`, `explosion.blast`, `explosion.shrapnel`, `explosion.ended`,
    /// `combat_text.shown`, `item.activated`, `item.thrown`, `item.landed`, `entity.damaged`,
    /// `terrain.changed`, `furniture.changed` or `field.changed`.
    std::string type = {};
    /// Opaque ID of the projectile or explosion; empty for combat text. An `item.landed` of an item
    /// nobody threw has none.
    std::string id = {};
    /// The path of a projectile or the squares of one blast ring, in drawing order.
    std::vector<position> cells = {};
    std::optional<position> at = std::nullopt;
    /// What is shown: the projectile, the item, or the terrain or furniture after the change.
    std::optional<look> appearance = std::nullopt;
    /// `terrain.changed` and `furniture.changed`: what the square held before.
    std::optional<look> previous = std::nullopt;
    /// `item.thrown`: where the item left from and where it was aimed.
    std::optional<position> origin = std::nullopt;
    std::optional<position> target = std::nullopt;
    /// Opaque ID of the creature that threw (`item.thrown`) or was hurt (`entity.damaged`).
    std::string entity = {};
    /// `entity.damaged`: the body part hit, when the creature has parts.
    std::string part = {};
    std::optional<std::uint64_t> amount = std::nullopt;
    /// `field.changed`: the field type, with `amount` its intensity afterwards (0 when gone).
    std::string field = {};
    /// `item.activated`: whole turns left on a fuse.
    std::optional<std::uint64_t> turns = std::nullopt;
    std::optional<std::uint64_t> radius = std::nullopt;
    std::string color = {};
    /// Tile id the native tiles draw an explosion with (`explosion`, `fd_smoke`, ...).
    std::string tile = {};
    std::vector<text_segment> segments = {};
    /// Combat text only: the unit step the native text scrolls by each animation step.
    std::optional<position> scroll = std::nullopt;
    /// Combat text only: `hp` for a creature's hit-point readout, which stays beside it.
    std::string kind = {};
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
auto record_explosion( const tripoint_bub_ms &at, int radius, const nc_color &color,
                       const std::string &tile ) -> void;
auto record_custom_explosion( const tripoint_bub_ms &at,
                              const std::map<tripoint_bub_ms, nc_color> &area,
                              const std::string &tile ) -> void;
/// The facts below are game events, not animations: they are published whenever a client listens and the
/// avatar sees the square, whatever the animation options say.
struct throw_options {
    const Creature &by;
    const item &thrown;
    tripoint_bub_ms from;
    tripoint_bub_ms to;
};
/// The item leaves the thrower's hand. The projectile ID it mints is the one the flight and the landing carry.
auto record_throw( const throw_options &options ) -> void;
/// A thrown or fired item comes to rest.
auto record_landing( const item &landed, const tripoint_bub_ms &at ) -> void;
struct activation_options {
    tripoint_bub_ms at;
    const item &lit;
    /// Whole turns left on a fuse; none for an item without one.
    std::optional<int> turns = std::nullopt;
};
auto record_activation( const activation_options &options ) -> void;
struct damage_options {
    const Creature &hurt;
    bodypart_id part;
    int amount = 0;
};
auto record_damage( const damage_options &options ) -> void;
struct square_terrain {
    ter_id terrain;
    furn_id furniture;
};
auto terrain_at( const tripoint_bub_ms &at ) -> square_terrain;
/// `terrain.changed` and `furniture.changed` for what the square holds now, against `before`.
auto record_terrain_change( const tripoint_bub_ms &at, const square_terrain &before ) -> void;
/// `field.changed` with the intensity the square holds of `field` now (0 when it is gone).
auto record_field( const tripoint_bub_ms &at, const field_type_id &field ) -> void;
/// Marks where the facts of one explosion step begin: the blast frame the step ends with goes before them.
auto begin_blast_step() -> void;
auto record_text( point at, direction scroll, const std::string &kind, const std::string &first,
                  game_message_type first_type, const std::string &second,
                  game_message_type second_type ) -> void;

} // namespace engine_client::presentation
