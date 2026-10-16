#pragma once

#include "animation.h"
#include "coordinates.h"
#include "engine_client_state.h"
#include "enums.h"
#include "type_id.h"
#include "units_angle.h"

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

class Creature;
class item;
namespace sfx
{
enum class channel : int;
} // namespace sfx

namespace engine_client
{

struct text_segment {
    std::string text = {};
    /// Native color name of the message type.
    std::string color = {};
    auto operator<=>( const text_segment & ) const = default; // *NOPAD*
};
/// What the native sound player (src/sdlsound.cpp) is asked to play, for a client to play from the sound pack.
struct sound_cue {
    /// The `sound_effect` id and variant of the sound pack; empty where a channel stops.
    std::string id = {};
    std::string variant = {};
    /// `sound.channel`: the reserved channel.
    std::string channel = {};
    /// `music.changed`: the playlist; empty where the music stops.
    std::string playlist = {};
    /// In mixer units, 128 the full gain, with the native volume option applied; the sound pack's own volume
    /// of the effect or playlist entry scales it as the native player does.
    int volume = 0;
    /// Clockwise degrees from straight ahead the native mixer places the sound at; none for no direction.
    std::optional<int> angle = std::nullopt;
    /// `sound.played`: false when the native player skips the sound while the same one still plays.
    bool stacks = true;
    /// `sound.channel`: how long the sound fades in when it starts, or out when it stops.
    int fade_ms = 0;
    /// `sound.channel`: how often the sound repeats after the first time; -1 until the channel stops.
    int loops = -1;
    auto operator<=>( const sound_cue & ) const = default; // *NOPAD*
};
/// One native animation fact: what the Tiles and curses renderers would draw, before any renderer.
/// Transient: it changes no state.
struct presentation_value {
    /// `projectile.moved`, `projectile.hit`, `explosion.started`, `explosion.blast`, `explosion.shrapnel`, `explosion.ended`,
    /// `combat_text.shown`, `item.activated`, `item.thrown`, `item.landed`, `entity.damaged`,
    /// `terrain.changed`, `furniture.changed`, `field.changed`, `sound.heard`, `entity.moved`, `entity.gone`,
    /// `sound.played`, `sound.channel` or `music.changed`.
    std::string type = {};
    /// Opaque ID of the projectile or explosion; empty for combat text. An `item.landed` of an item
    /// no attack launched has none.
    std::string id = {};
    /// The path of a projectile or the squares of one blast ring, in drawing order.
    std::vector<position> cells = {};
    std::optional<position> at = std::nullopt;
    /// What is shown: the projectile, the item, or the terrain or furniture after the change.
    std::optional<look> appearance = std::nullopt;
    /// `terrain.changed` and `furniture.changed`: what the square held before.
    std::optional<look> previous = std::nullopt;
    /// `item.thrown` and `entity.moved`: where the item or creature left from (`entity.moved` leaves it out
    /// when the avatar did not see that square) and, for a throw, where it was aimed.
    std::optional<position> origin = std::nullopt;
    std::optional<position> target = std::nullopt;
    /// Opaque ID of the creature that threw (`item.thrown`), was hit (`projectile.hit`, when the avatar sees it) or was hurt
    /// (`entity.damaged`).
    std::string entity = {};
    /// `entity.damaged`: the body part hit, when the creature has parts.
    std::string part = {};
    /// The damage of `entity.damaged`, the intensity of `field.changed`, the decibels of `sound.heard`.
    std::optional<std::uint64_t> amount = std::nullopt;
    /// `sound.heard`: what the avatar hears.
    std::string text = {};
    /// `sound.heard`: the native message log also has a line for this sound.
    bool logged = false;
    /// `sound.heard`: where the native message says the sound comes from. Absent when the sound is at the avatar's
    /// square or the avatar sees its source, where the message names no direction.
    std::optional<heading> from = std::nullopt;
    /// `sound.heard`: squares between the avatar and the source, counted as the native sound code does.
    std::optional<std::uint64_t> distance = std::nullopt;
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
    /// Combat text: `hp` for a creature's hit-point readout, which stays beside it. `entity.moved`: `walk` for a
    /// step to a neighboring square, `propelled` for any longer move. `entity.gone`: `lost_sight`, `died` or `removed`.
    std::string kind = {};
    /// How long the native animation holds this fact, from the ANIMATION_DELAY option.
    std::uint64_t duration_ms = 0;
    /// `sound.played`, `sound.channel` and `music.changed`.
    std::optional<sound_cue> sound = std::nullopt;
    auto operator<=>( const presentation_value & ) const = default; // *NOPAD*
};

} // namespace engine_client

/// Hooks at the renderer-independent animation entry points (src/animation.cpp, src/output.cpp).
/// Nothing is recorded until a session starts collecting.
namespace engine_client::presentation
{

/// The most facts of the busy kinds (creatures moving, terrain and fields changing) one `take` holds. The
/// state delta of the boundary carries what the rest changed.
constexpr auto maximum_ambient_facts = std::size_t { 256 };

auto collect( bool on ) -> void;
/// Whether a client is listening: the cheap test before work done only for a client.
auto collecting() -> bool;
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
/// One projectile attack. Its throw, flight, hits and landing share one ID; a nested attack gives the outer
/// one back when it ends.
class flight_scope
{
    public:
        flight_scope();
        ~flight_scope();
        flight_scope( const flight_scope & ) = delete;
        auto operator=( const flight_scope & ) -> flight_scope & = delete; // *NOPAD*
    private:
        std::string outer_id_;
        std::optional<std::size_t> outer_start_;
};
/// How many facts are pending now: where the facts an event causes begin.
auto mark() -> std::size_t;
struct hit_options {
    tripoint_bub_ms at;
    /// The creature struck; none for an obstacle.
    const Creature *struck = nullptr;
    /// The `mark` taken before the hit did anything: the hit goes before its effects.
    std::size_t mark = 0;
};
/// The projectile of the current `flight_scope` strikes a creature or an obstacle.
auto record_hit( const hit_options &options ) -> void;
/// A thrown or fired item comes to rest.
auto record_landing( const item &landed, const tripoint_bub_ms &at ) -> void;
struct activation_options {
    tripoint_bub_ms at;
    const item &lit;
    /// Whole turns left on a fuse; none for an item without one.
    std::optional<int> turns = std::nullopt;
};
auto record_activation( const activation_options &options ) -> void;
struct sound_options {
    /// What the avatar makes of the sound, as the native message names it.
    const std::string &text;
    int volume = 0;
    /// The square the sound was made on.
    tripoint_bub_ms origin;
    /// The square the native game marks the sound on: never the source, and none when the avatar sees it.
    std::optional<tripoint_bub_ms> marker = std::nullopt;
    /// The native game wrote a message log line for the sound.
    bool logged = false;
};
/// A sound reached the avatar, past its hearing, sleep and deafness.
auto record_sound( const sound_options &options ) -> void;
/// The native sound player's calls (src/sdlsound.cpp), told whether or not this process plays audio. Like the
/// native player, nothing is played while the SOUND_ENABLED option is off or at a volume of 0.
struct sound_effect_options {
    const std::string &id;
    const std::string &variant;
    /// The volume of the native call, 0-100.
    int volume = 0;
    std::optional<units::angle> angle = std::nullopt;
    bool stacks = true;
};
/// `sound.played`: one sound effect.
auto record_sound_effect( const sound_effect_options &options ) -> void;
struct channel_sound_options {
    sfx::channel channel;
    const std::string &id;
    const std::string &variant;
    int volume = 0;
    int fade_in_ms = 0;
    int loops = -1;
};
/// `sound.channel`: a reserved channel starts a sound, unless it plays one that repeats until it stops.
auto record_channel_sound( const channel_sound_options &options ) -> void;
/// `sound.channel` without a sound for a channel that plays one; `any` stops every channel.
auto record_channel_stop( sfx::channel channel, int fade_ms ) -> void;
struct channel_change_options {
    sfx::channel channel;
    std::optional<int> volume = std::nullopt;
    std::optional<units::angle> angle = std::nullopt;
};
/// `sound.channel` again with the new mixer volume or direction of what a channel plays.
auto record_channel_change( const channel_change_options &options ) -> void;
/// Whether a channel plays a sound that repeats until it stops: what the native player answers without audio.
auto channel_playing( sfx::channel channel ) -> bool;
/// `music.changed`: the playlist the native player plays now, empty when it stops.
auto record_music( const std::string &playlist ) -> void;
struct move_options {
    const Creature &mover;
    tripoint_bub_ms from;
    tripoint_bub_ms to;
};
/// A creature stepped or was moved: `entity.moved` where the avatar sees it arrive, `entity.gone` where it left
/// the avatar's sight.
auto record_move( const move_options &options ) -> void;
/// A creature the avatar sees died.
auto record_death( const Creature &dead ) -> void;
struct damage_options {
    const Creature &hurt;
    bodypart_id part;
    int amount = 0;
};
auto record_damage( const damage_options &options ) -> void;
/// The map boundary (src/map/mapbuffer.cpp) tells what changed on the avatar's level: a terrain, a
/// furniture, or a field a map call added, removed or set. The avatar must see the square.
auto record_terrain_change( const tripoint_bub_ms &at, const ter_id &from,
                            const ter_id &to ) -> void;
auto record_furniture_change( const tripoint_bub_ms &at, const furn_id &from,
                              const furn_id &to ) -> void;
/// `field.changed` with the intensity the square holds of `field` now (0 when it is gone).
auto record_field( const tripoint_bub_ms &at, const field_type_id &field ) -> void;
/// Fields are also changed in place by the field turn (spreading, decay, drift), which no map call sees.
/// While a watch lives, the fields the avatar sees are compared at its start and end instead, and the
/// map calls in between are not told one by one.
class field_watch
{
    public:
        field_watch();
        ~field_watch();
        field_watch( const field_watch & ) = delete;
        auto operator=( const field_watch & ) -> field_watch & = delete; // *NOPAD*
    private:
        std::optional<std::map<tripoint_bub_ms, std::map<field_type_id, int>>> before_;
};
/// Marks where the facts of one explosion step begin: the blast frame the step ends with goes before them.
auto begin_blast_step() -> void;
auto record_text( point at, direction scroll, const std::string &kind, const std::string &first,
                  game_message_type first_type, const std::string &second,
                  game_message_type second_type ) -> void;

} // namespace engine_client::presentation
