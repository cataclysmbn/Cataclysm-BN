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
#include "map/submap.h"
#include "game_constants.h"
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
    /// The projectile of the item being thrown, until its attack begins.
    std::string thrown_projectile = {};
    /// The projectile of the attack in progress, and where its facts begin.
    std::string current_projectile = {};
    std::optional<std::size_t> flight_start = std::nullopt;
    std::size_t blast_step_start = 0;
    /// Busy facts pushed since the last `take`, against `maximum_ambient_facts`.
    std::size_t ambient = 0;
    /// Live `field_watch`es: the map calls they cover are not told one by one.
    int watching = 0;
    /// The creatures the client was last told it sees.
    std::set<std::string> known = {};
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
/// A fact of a busy kind: a crowded turn keeps the first `maximum_ambient_facts` of them.
auto push_ambient( presentation_value fact ) -> void
{
    auto &feed = state();
    if( feed.ambient >= maximum_ambient_facts ) { return; }
    ++feed.ambient;
    push( std::move( fact ) );
}
/// One native animation step holds this long.
auto step_ms() -> std::uint64_t
{
    return static_cast<std::uint64_t>( std::max( 0, get_option<int>( "ANIMATION_DELAY" ) ) );
}
auto new_projectile( feed_state &feed ) -> std::string
{
    return "projectile:" + std::to_string( ++feed.next_projectile );
}
/// A shot flies under the ID of the attack in progress, else under a new one.
auto shot_id( feed_state &feed ) -> std::string
{
    return feed.current_projectile.empty() ? new_projectile( feed ) : feed.current_projectile;
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

auto collecting() -> bool { return state().collecting; }

auto collect( const bool on ) -> void
{
    auto &feed = state();
    feed.collecting = on;
    if( !on ) {
        feed.pending.clear();
        feed.known.clear();
    }
}

auto take() -> std::vector<presentation_value>
{
    auto &feed = state();
    feed.thrown_projectile.clear();
    feed.flight_start.reset();
    feed.ambient = 0;
    if( feed.collecting ) {
        // Creatures that dropped out of the disclosed set without a step or a death of their own.
        auto now = world::disclosed_entity_ids();
        for( const auto &id : feed.known ) {
            if( now.contains( id ) ) { continue; }
            push( { .type = "entity.gone", .entity = id, .kind = world::entity_alive( id ) ? "lost_sight" : "removed" } );
        }
        feed.known = std::move( now );
    }
    return std::exchange( feed.pending, {} );
}

auto record_bullet( const bullet_step &step ) -> void
{
    auto &feed = state();
    if( !feed.collecting || !seen( step.at ) ) { return; }
    // The native loop draws one square per call; a lower index than the last call starts a new shot.
    if( step.index <= feed.last_bullet_index || feed.last_projectile.empty() ) {
        feed.last_projectile = shot_id( feed );
    }
    feed.last_bullet_index = step.index;
    push( { .type = "projectile.moved", .id = feed.last_projectile, .cells = { world::position_at( step.at ) },
            .appearance = bullet_look( step.bullet, step.custom_sprite ), .duration_ms = step_ms() } );
}

auto record_trajectories( const draw_bullet_trajectories_options &options ) -> void
{
    auto &feed = state();
    if( !feed.collecting ) { return; }
    const auto appearance = bullet_look( options.bullet, options.custom_sprite );
    for( const auto &trajectory : options.trajectories ) {
        const auto visible = only_seen( trajectory );
        if( visible.empty() ) { continue; }
        // A line is drawn at once; a shot flies one square per animation step.
        const auto steps = options.draw_as_line ? 1 : trajectory.size();
        push( { .type = "projectile.moved", .id = shot_id( feed ),
                .cells = path_of( visible ), .appearance = appearance, .duration_ms = step_ms() * steps } );
    }
    feed.last_bullet_index = -1;
    feed.last_projectile.clear();
}

auto record_line( const draw_sprite_line_options &options ) -> void
{
    auto &feed = state();
    if( !feed.collecting ) { return; }
    // The caller resizes its trajectory past the end, which leaves value-initialized squares.
    auto points = options.points;
    std::erase( points, tripoint_bub_ms::zero() );
    points = only_seen( points );
    if( points.empty() ) { return; }
    // The line is drawn once the attack is over; it flew before whatever the attack hit.
    auto flight = presentation_value{ .type = "projectile.moved", .id = shot_id( feed ),
                                      .cells = path_of( points ), .appearance = bullet_look( '*', options.sprite ),
                                      .duration_ms = step_ms() };
    if( !feed.flight_start || feed.pending.size() >= maximum_pending ) { return push( std::move( flight ) ); }
    feed.pending.insert( feed.pending.begin() + std::min( *feed.flight_start, feed.pending.size() ),
                         std::move( flight ) );
}

auto begin_blast( const tripoint_bub_ms &at, const int radius, const bool fiery ) -> std::string
{
    auto &feed = state();
    if( !feed.collecting ) { return {}; }
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
    auto frames = std::vector<presentation_value> {};
    if( auto held = feed.held.extract( id ) ) {
        feed.announced.insert( id );
        frames.push_back( std::move( held.mapped() ) );
    }
    // The native pacing: one logical time unit lasts ten animation delays.
    const auto frame = step_ms() * 10;
    if( !blasted.empty() ) {
        frames.push_back( { .type = "explosion.blast", .id = id, .cells = path_of( blasted ), .duration_ms = frame } );
    }
    if( !shrapneled.empty() ) {
        frames.push_back( { .type = "explosion.shrapnel", .id = id, .cells = path_of( shrapneled ), .duration_ms = frame } );
    }
    // What the step did (damage, terrain, fields) follows the frame that shows it.
    if( feed.pending.size() >= maximum_pending ) { return; }
    const auto at = std::min( feed.blast_step_start, feed.pending.size() );
    feed.pending.insert( feed.pending.begin() + at, std::make_move_iterator( frames.begin() ),
                         std::make_move_iterator( frames.end() ) );
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
    if( !feed.collecting || !seen( at ) ) { return; }
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
    if( !feed.collecting ) { return; }
    const auto cells = only_seen( area | std::views::keys | std::ranges::to<std::vector>() );
    if( cells.empty() ) { return; }
    const auto id = "explosion:" + std::to_string( ++feed.next_explosion );
    push( { .type = "explosion.started", .id = id, .at = world::position_at( at ), .tile = tile,
            .duration_ms = step_ms() } );
    push( { .type = "explosion.blast", .id = id, .cells = path_of( cells ), .duration_ms = step_ms() } );
    push( { .type = "explosion.ended", .id = id } );
}

auto record_throw( const throw_options &options ) -> void
{
    auto &feed = state();
    if( !feed.collecting || !( seen( options.from ) || seen( options.to ) ) ) { return; }
    auto by = world::entity_id( options.by );
    if( by.empty() ) { return; }
    feed.thrown_projectile = new_projectile( feed );
    push( { .type = "item.thrown", .id = feed.thrown_projectile, .appearance = world::item_appearance( options.thrown ),
            .origin = world::position_at( options.from ), .target = world::position_at( options.to ),
            .entity = std::move( by ) } );
}

flight_scope::flight_scope() : outer_id_( state().current_projectile ),
    outer_start_( state().flight_start )
{
    auto &feed = state();
    if( !feed.collecting ) { return; }
    // A throw has minted the ID of its flight already.
    feed.current_projectile = feed.thrown_projectile.empty() ? new_projectile( feed ) :
                              std::exchange( feed.thrown_projectile, {} );
    feed.last_projectile = feed.current_projectile;
    feed.last_bullet_index = -1;
    feed.flight_start = feed.pending.size();
}

flight_scope::~flight_scope()
{
    auto &feed = state();
    feed.current_projectile = std::move( outer_id_ );
    feed.flight_start = outer_start_;
}

auto mark() -> std::size_t { return state().pending.size(); }

auto record_hit( const hit_options &options ) -> void
{
    auto &feed = state();
    if( !feed.collecting || !seen( options.at ) || feed.pending.size() >= maximum_pending ) { return; }
    const auto disclosed = options.struck != nullptr && get_avatar().sees( *options.struck );
    feed.pending.insert( feed.pending.begin() + std::min( options.mark, feed.pending.size() ), {
        .type = "projectile.hit", .id = feed.current_projectile, .at = world::position_at( options.at ),
        .entity = disclosed ? world::entity_id( *options.struck ) : std::string{}
    } );
}

auto record_landing( const item &landed, const tripoint_bub_ms &at ) -> void
{
    auto &feed = state();
    if( !feed.collecting || !seen( at ) ) { return; }
    push( { .type = "item.landed", .id = feed.current_projectile, .at = world::position_at( at ),
            .appearance = world::item_appearance( landed ) } );
}

auto record_activation( const activation_options &options ) -> void
{
    if( !state().collecting || !seen( options.at ) ) { return; }
    push( { .type = "item.activated", .at = world::position_at( options.at ),
            .appearance = world::item_appearance( options.lit ),
            .turns = options.turns ? std::optional<std::uint64_t>( std::max( 0, *options.turns ) ) : std::nullopt } );
}

auto record_sound( const sound_options &options ) -> void
{
    if( !state().collecting ) { return; }
    const auto &you = get_avatar();
    // The native message names a direction unless the sound is at the avatar's square or its source is in sight.
    const auto named = options.origin != you.bub_pos() && !you.sees( options.origin );
    push( { .type = "sound.heard", .at = options.marker ? std::optional( world::position_at( *options.marker ) ) : std::nullopt,
            .amount = static_cast<std::uint64_t>( std::max( 0, options.volume ) ), .text = options.text,
            .logged = options.logged,
            .from = named ? std::optional( world::heading_of( direction_from( you.bub_pos(), options.origin ) ) ) : std::nullopt,
            .distance = static_cast<std::uint64_t>( rl_dist( you.bub_pos(), options.origin ) ) } );
}

auto record_move( const move_options &options ) -> void
{
    auto &feed = state();
    if( !feed.collecting || feed.ambient >= maximum_ambient_facts || options.mover.is_avatar() ) { return; }
    const auto left = get_map().inbounds( options.from ) && seen( options.from );
    const auto arrived = get_avatar().sees( options.mover );
    if( !left && !arrived ) { return; }
    auto id = world::entity_id( options.mover );
    if( id.empty() ) { return; }
    if( arrived ) {
        push_ambient( { .type = "entity.moved", .at = world::position_at( options.to ),
                        .origin = left ? std::optional( world::position_at( options.from ) ) : std::nullopt,
                        .entity = std::move( id ), .kind = rl_dist( options.from, options.to ) <= 1 ? "walk" : "propelled" } );
    } else {
        feed.known.erase( id );
        push_ambient( { .type = "entity.gone", .entity = std::move( id ), .kind = "lost_sight" } );
    }
}

auto record_death( const Creature &dead ) -> void
{
    auto &feed = state();
    if( !feed.collecting || dead.is_avatar() || !seen( dead.bub_pos() ) ) { return; }
    auto id = world::entity_id( dead );
    if( id.empty() ) { return; }
    feed.known.erase( id );
    push( { .type = "entity.gone", .entity = std::move( id ), .kind = "died" } );
}

auto record_damage( const damage_options &options ) -> void
{
    if( !state().collecting || options.amount <= 0 || !seen( options.hurt.bub_pos() ) ) { return; }
    auto hurt = world::entity_id( options.hurt );
    if( hurt.empty() ) { return; }
    push( { .type = "entity.damaged", .entity = std::move( hurt ),
            .part = options.hurt.as_character() ? options.part.id().str() : std::string{},
            .amount = static_cast<std::uint64_t>( options.amount ) } );
}

auto record_terrain_change( const tripoint_bub_ms &at, const ter_id &from,
                            const ter_id &to ) -> void
{
    auto &feed = state();
    if( !feed.collecting || feed.ambient >= maximum_ambient_facts || !seen( at ) ) { return; }
    push_ambient( { .type = "terrain.changed", .at = world::position_at( at ),
                    .appearance = world::terrain_appearance( to ), .previous = world::terrain_appearance( from ) } );
}

auto record_furniture_change( const tripoint_bub_ms &at, const furn_id &from,
                              const furn_id &to ) -> void
{
    auto &feed = state();
    if( !feed.collecting || feed.ambient >= maximum_ambient_facts || !seen( at ) ) { return; }
    push_ambient( { .type = "furniture.changed", .at = world::position_at( at ),
                    .appearance = world::furniture_appearance( to ), .previous = world::furniture_appearance( from ) } );
}

auto record_field( const tripoint_bub_ms &at, const field_type_id &field ) -> void
{
    auto &feed = state();
    if( !feed.collecting || feed.watching > 0 || feed.ambient >= maximum_ambient_facts ||
        !seen( at ) ) { return; }
    push_ambient( { .type = "field.changed", .at = world::position_at( at ), .field = field.id().str(),
                    .amount = static_cast<std::uint64_t>( std::max( 0, get_map().get_field_intensity( at, field ) ) ) } );
}

namespace
{
using field_cells = std::map<tripoint_bub_ms, std::map<field_type_id, int>>;
/// The fields on the squares the avatar sees on its level, by square and type, with their intensity.
auto seen_fields() -> field_cells
{
    auto cells = field_cells{};
    const auto &here = get_map();
    const auto z = get_avatar().bub_pos().z();
    for( const auto x : std::views::iota( 0, here.getmapsize() ) ) {
        for( const auto y : std::views::iota( 0, here.getmapsize() ) ) {
            const auto origin = tripoint_bub_ms( x * SEEX, y * SEEY, z );
            const auto *sm = here.get_submap_at( origin );
            if( sm == nullptr || sm->field_count == 0 ) { continue; }
            for( const auto &local : sm->field_cache ) {
                const auto at = origin + tripoint_rel_ms( local.x(), local.y(), 0 );
                if( !seen( at ) ) { continue; }
                for( const auto &[type, entry] : sm->get_field( local ) ) {
                    if( entry.get_field_intensity() > 0 ) { cells[at][type] = entry.get_field_intensity(); }
                }
            }
        }
    }
    return cells;
}
} // namespace

field_watch::field_watch()
{
    auto &feed = state();
    if( !feed.collecting ) { return; }
    ++feed.watching;
    before_ = seen_fields();
}

field_watch::~field_watch()
{
    if( !before_ ) { return; }
    --state().watching;
    const auto after = seen_fields();
    const auto intensity_of = []( const field_cells & cells, const tripoint_bub_ms & at,
    const field_type_id & type ) {
        const auto cell = cells.find( at );
        if( cell == cells.end() ) { return 0; }
        const auto field = cell->second.find( type );
        return field == cell->second.end() ? 0 : field->second;
    };
    const auto tell = [&]( const tripoint_bub_ms & at, const field_type_id & type,
    const int intensity ) {
        push_ambient( { .type = "field.changed", .at = world::position_at( at ), .field = type.id().str(),
                        .amount = static_cast<std::uint64_t>( intensity ) } );
    };
    for( const auto &[at, fields] : after ) {
        for( const auto &[type, now] : fields ) {
            if( now != intensity_of( *before_, at, type ) ) { tell( at, type, now ); }
        }
    }
    for( const auto &[at, fields] : *before_ ) {
        for( const auto &[type, was] : fields ) {
            if( intensity_of( after, at, type ) == 0 ) { tell( at, type, 0 ); }
        }
    }
}

auto begin_blast_step() -> void
{
    auto &feed = state();
    feed.blast_step_start = feed.pending.size();
}

auto record_text( const point at, const direction scroll, const std::string &kind,
                  const std::string &first,
                  const game_message_type first_type,
                  const std::string &second, const game_message_type second_type ) -> void
{
    if( !state().collecting || !( test_mode || get_option<bool>( "ANIMATION_SCT" ) ) ) {
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
