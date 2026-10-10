#include "engine_client_presentation.h"

#include <algorithm>
#include <map>
#include <set>
#include <ranges>

#include "cached_options.h"
#include "engine_client_world.h"
#include "line.h"
#include "avatar.h"
#include "game.h"
#include "map/map.h"
#include "messages.h"
#include "output.h"
#include "options.h"

namespace engine_client::presentation
{
namespace
{

constexpr auto maximum_pending = std::size_t { 4096 };

struct feed_state {
    bool collecting = false;
    std::uint64_t next_projectile = 0;
    std::uint64_t next_explosion = 0;
    int last_bullet_index = -1;
    std::string last_projectile = {};
    std::vector<presentation_value> pending = {};
    /// Blasts whose start is held back until something of them is seen, and those already announced.
    std::map<std::string, presentation_value> held = {};
    std::set<std::string> announced = {};
};
auto state() -> feed_state & // *NOPAD*
{
    static auto value = feed_state{};
    return value;
}

auto push( presentation_value fact ) -> void
{
    auto &feed = state();
    if( feed.pending.size() < maximum_pending ) { feed.pending.push_back( std::move( fact ) ); }
}
/// The native game draws animations only with ANIMATIONS on; only tests force them on.
auto animated() -> bool
{
    return test_mode || get_option<bool>( "ANIMATIONS" );
}
/// One native animation step holds this long.
auto step_ms() -> std::uint64_t
{
    return static_cast<std::uint64_t>( std::max( 0, get_option<int>( "ANIMATION_DELAY" ) ) );
}
auto bullet_look( const char bullet, const std::string &custom_sprite ) -> look
{
    return { .kind = "projectile", .id = custom_sprite.empty() ? std::nullopt :
                                         std::optional<std::string>( custom_sprite ),
             .glyph = std::string( 1, bullet ), .color = world::color_name( c_red ) };
}
/// What the native animations draw: only squares the avatar sees.
auto seen( const tripoint_bub_ms &at ) -> bool
{
    return get_avatar().sees( at );
}
auto only_seen( const std::vector<tripoint_bub_ms> &points ) -> std::vector<tripoint_bub_ms>
{
    return points | std::views::filter( seen ) | std::ranges::to<std::vector>();
}
auto path_of( const std::vector<tripoint_bub_ms> &points ) -> std::vector<position>
{
    return points | std::views::transform( world::position_at ) | std::ranges::to<std::vector>();
}

} // namespace

auto collect( const bool on ) -> void
{
    auto &feed = state();
    feed.collecting = on;
    if( !on ) { feed.pending.clear(); }
}

auto take() -> std::vector<presentation_value>
{
    return std::exchange( state().pending, {} );
}

auto record_bullet( const bullet_step &step ) -> void
{
    auto &feed = state();
    if( !feed.collecting || !animated() || !seen( step.at ) ) { return; }
    // The native loop draws one square per call; a lower index than the last call starts a new shot.
    if( step.index <= feed.last_bullet_index || feed.last_projectile.empty() ) {
        feed.last_projectile = "projectile:" + std::to_string( ++feed.next_projectile );
    }
    feed.last_bullet_index = step.index;
    push( { .type = "projectile.moved", .id = feed.last_projectile, .cells = { world::position_at( step.at ) },
            .appearance = bullet_look( step.bullet, step.custom_sprite ), .duration_ms = step_ms() } );
}

auto record_trajectories( const draw_bullet_trajectories_options &options ) -> void
{
    auto &feed = state();
    if( !feed.collecting || !animated() ) { return; }
    const auto appearance = bullet_look( options.bullet, options.custom_sprite );
    for( const auto &trajectory : options.trajectories ) {
        const auto visible = only_seen( trajectory );
        if( visible.empty() ) { continue; }
        // A line is drawn at once; a shot flies one square per animation step.
        const auto steps = options.draw_as_line ? 1 : trajectory.size();
        push( { .type = "projectile.moved", .id = "projectile:" + std::to_string( ++feed.next_projectile ),
                .cells = path_of( visible ), .appearance = appearance, .duration_ms = step_ms() * steps } );
    }
    feed.last_bullet_index = -1;
    feed.last_projectile.clear();
}

auto record_line( const draw_sprite_line_options &options ) -> void
{
    auto &feed = state();
    if( !feed.collecting || !animated() ) { return; }
    // The caller resizes its trajectory past the end, which leaves value-initialized squares.
    auto points = options.points;
    std::erase( points, tripoint_bub_ms::zero() );
    points = only_seen( points );
    if( points.empty() ) { return; }
    push( { .type = "projectile.moved", .id = "projectile:" + std::to_string( ++feed.next_projectile ),
            .cells = path_of( points ), .appearance = bullet_look( '*', options.sprite ),
            .duration_ms = step_ms() } );
}

auto begin_blast( const tripoint_bub_ms &at, const int radius, const bool fiery ) -> std::string
{
    auto &feed = state();
    if( !feed.collecting || !animated() ) { return {}; }
    auto id = "explosion:" + std::to_string( ++feed.next_explosion );
    auto started = presentation_value{ .type = "explosion.started", .id = id, .at = world::position_at( at ),
                                       .radius = static_cast<std::uint64_t>( std::max( 0, radius ) ),
                                       .color = world::color_name( fiery ? c_red : c_white ), .tile = "explosion" };
    // An explosion nobody sees is announced by its first seen square, if it ever has one.
    if( seen( at ) ) {
        feed.announced.insert( id );
        push( std::move( started ) );
    } else {
        feed.held.emplace( id, std::move( started ) );
    }
    return id;
}

auto record_blast_frame( const std::string &id, const std::vector<tripoint_bub_ms> &blast,
                         const std::vector<tripoint_bub_ms> &shrapnel ) -> void
{
    if( id.empty() ) { return; }
    auto &feed = state();
    const auto blasted = only_seen( blast );
    const auto shrapneled = only_seen( shrapnel );
    if( blasted.empty() && shrapneled.empty() ) { return; }
    if( const auto held = feed.held.extract( id ) ) {
        feed.announced.insert( id );
        push( std::move( held.mapped() ) );
    }
    // The native pacing: one logical time unit lasts ten animation delays.
    const auto frame = step_ms() * 10;
    if( !blasted.empty() ) {
        push( { .type = "explosion.blast", .id = id, .cells = path_of( blasted ), .duration_ms = frame } );
    }
    if( !shrapneled.empty() ) {
        push( { .type = "explosion.shrapnel", .id = id, .cells = path_of( shrapneled ), .duration_ms = frame } );
    }
}

auto end_blast( const std::string &id ) -> void
{
    auto &feed = state();
    feed.held.erase( id );
    if( feed.announced.erase( id ) > 0 ) { push( { .type = "explosion.ended", .id = id } ); }
}

auto record_explosion( const tripoint_bub_ms &at, const int radius, const nc_color &color,
                       const std::string &tile ) -> void
{
    auto &feed = state();
    if( !feed.collecting || !animated() || !seen( at ) ) { return; }
    const auto id = "explosion:" + std::to_string( ++feed.next_explosion );
    push( { .type = "explosion.started", .id = id, .at = world::position_at( at ),
            .radius = static_cast<std::uint64_t>( std::max( 0, radius ) ), .color = world::color_name( color ), .tile = tile,
            .duration_ms = step_ms() * ( std::max( 0, radius ) + 1 ) } );
    push( { .type = "explosion.ended", .id = id } );
}

auto record_custom_explosion( const tripoint_bub_ms &at,
                              const std::map<tripoint_bub_ms, nc_color> &area,
                              const std::string &tile ) -> void
{
    auto &feed = state();
    if( !feed.collecting || !animated() ) { return; }
    const auto cells = only_seen( area | std::views::keys | std::ranges::to<std::vector>() );
    if( cells.empty() ) { return; }
    const auto id = "explosion:" + std::to_string( ++feed.next_explosion );
    push( { .type = "explosion.started", .id = id, .at = world::position_at( at ), .tile = tile,
            .duration_ms = step_ms() } );
    push( { .type = "explosion.blast", .id = id, .cells = path_of( cells ), .duration_ms = step_ms() } );
    push( { .type = "explosion.ended", .id = id } );
}

auto record_text( const point at, const direction scroll, const std::string &kind,
                  const std::string &first,
                  const game_message_type first_type,
                  const std::string &second, const game_message_type second_type ) -> void
{
    if( !state().collecting || !animated() || !( test_mode || get_option<bool>( "ANIMATION_SCT" ) ) ) {
        return;
    }
    const auto square = tripoint_bub_ms( at.x, at.y, get_avatar().bub_pos().z() );
    if( !seen( square ) ) { return; }
    auto segments = std::vector<text_segment> { { first, world::color_name( msgtype_to_color( first_type ) ) } };
    if( !second.empty() ) {
        segments.push_back( { second, world::color_name( msgtype_to_color( second_type ) ) } );
    }
    push( { .type = "combat_text.shown",
            .at = world::position_at( square ),
            .segments = std::move( segments ),
            .scroll = position{ .x = displace_XY( scroll ).x, .y = displace_XY( scroll ).y }, .kind = kind,
            .duration_ms = step_ms() * scrollingcombattext::iMaxSteps } );
}

} // namespace engine_client::presentation
