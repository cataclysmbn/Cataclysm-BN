#include "map_perception.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <ranges>
#include <random>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "avatar.h"
#include "cata_utility.h"
#include "enums.h"
#include "map/field.h"
#include "game.h"
#include "map/map.h"
#include "map_memory.h"
#include "map/mapdata.h"
#include "map/submap.h"
#include "overmap/overmapbuffer.h"
#include "overmap/overmapbuffer_registry.h"
#include "output.h"
#include "trap.h"
#include "vehicle/vehicle.h"
#include "vehicle/vehicle_part.h"
#include "vehicle/vpart_position.h"
#include "vehicle/vpart_range.h"
#include "tileray.h"

namespace map_perception
{
namespace
{

const auto neighbors = std::array { point_south, point_east, point_west, point_north };

struct field_appearance {
    field_type_id field;
    ter_id terrain;
    furn_id furniture;
    auto operator<=>( const field_appearance & ) const = default; // *NOPAD*
};

struct vehicle_appearance {
    memorized_terrain_tile tile;
    int symbol = 0;
    bool stationary = false;
    bool detailed = false;
    auto operator==( const vehicle_appearance & ) const -> bool = default;
};

/// This game-thread cache owns no submap/vehicle pointers or authoritative world work.
struct acquisition_state {
    const map *owner = nullptr;
    point_abs_sm origin{};
    dimension_id dimension{};
    tripoint_abs_ms position{};
    int size = 0;
    bool full_pending = true;
    bool committing = false;
    bool blind = false;
    float threshold = 0;
    visibility_variables vision{};
    std::set<tripoint_bub_ms> dirty;
    // Keep hidden/unvisited invalidations until a visibility completion can authorize them.
    std::set<tripoint_bub_ms> pending;
    std::map<tripoint_bub_ms, field_appearance> fields;
    std::map<tripoint_bub_ms, trap_id> traps;
    std::map<tripoint_bub_ms, vehicle_appearance> vehicles;
};

auto state = acquisition_state {};

auto same_vision( const visibility_variables &a, const visibility_variables &b ) -> bool
{
    return a.variables_set == b.variables_set && a.u_sight_impaired == b.u_sight_impaired &&
           a.u_is_boomered == b.u_is_boomered && a.g_light_level == b.g_light_level &&
           a.u_clairvoyance == b.u_clairvoyance && a.u_unimpaired_range == b.u_unimpaired_range &&
           a.vision_threshold == b.vision_threshold && a.visibility_range == b.visibility_range &&
           a.detail_range == b.detail_range && a.visibility_scale_factor == b.visibility_scale_factor;
}

/// Compare sparse native field/vehicle inputs, not every loaded map cell. In particular,
/// direct field aging and vehicle control/appearance changes need no visibility invalidation.
template<typename Snapshot>
auto update_sparse( const map &here, Snapshot &previous, Snapshot current ) -> void
{
    for( const auto &[p, value] : current ) {
        const auto old = previous.find( p );
        if( old == previous.end() || old->second != value ) {
            invalidate_cell( here, p );
        }
    }
    for( const auto &[p, value] : previous ) {
        if( !current.contains( p ) ) {
            invalidate_cell( here, p );
        }
    }
    previous = std::move( current );
}

struct cell_facts {
    tripoint_bub_ms position;
    ter_id terrain{};
    furn_id furniture{};
    trap_id trap{};
    int field_symbol = 0;
    bool field_overrides = false;
    bool vehicle_only = false;
    bool roof = false;
    bool stationary_vehicle = false;
    memorized_terrain_tile vehicle_tile{};
    int vehicle_symbol = 0;
};

auto normally_visible( const map &here, const tripoint_bub_ms &p ) -> bool
{
    if( !here.inbounds( p ) ) {
        return false;
    }
    const auto &you = get_avatar();
    if( you.is_blind() && ( you.clairvoyance() == 0 ||
                            rl_dist( you.bub_pos(), p ) > you.clairvoyance() ) ) {
        return false;
    }
    const auto &cache = here.access_cache( p.z() );
    return here.get_visibility( cache.visibility_cache[cache.idx( p.x(), p.y() )],
                                here.get_visibility_variables_cache() ) == VIS_CLEAR;
}

/// Only semantic historical knowledge can supply an invisible neighbor, never live terrain.
auto known_terrain( const map &here, const tripoint_bub_ms &p ) -> ter_id
{
    if( !here.inbounds( p ) ) {
        return ter_id();
    }
    if( normally_visible( here, p ) ) {
        return here.ter( p );
    }
    const auto abs = map_local_to_abs( here, p );
    const auto remembered = get_avatar().get_terrain_tile( abs );
    // Older Tiles memory stored an explicit terrain ID in the overlay slot.
    // This is a historical fact, unlike a glyph-only cell; do not rewrite it.
    const auto tile = remembered.tile.empty() ? get_avatar().get_memorized_tile( abs ).tile :
                      remembered.tile;
    const auto id = ter_str_id( tile );
    return !tile.empty() && id.is_valid() ? id.id() : ter_id();
}

auto known_furniture( const map &here, const tripoint_bub_ms &p ) -> furn_id
{
    if( !here.inbounds( p ) ) {
        return furn_id();
    }
    if( normally_visible( here, p ) ) {
        return here.furn( p );
    }
    const auto remembered = get_avatar().get_memorized_tile( map_local_to_abs( here, p ) );
    const auto id = furn_str_id( remembered.tile );
    return !remembered.tile.empty() && id.is_valid() ? id.id() : furn_id();
}

auto known_trap( const map &here, const tripoint_bub_ms &p ) -> trap_id
{
    if( !here.inbounds( p ) ) {
        return trap_id();
    }
    if( normally_visible( here, p ) ) {
        const auto &tr = here.tr_at( p );
        return tr.can_see( p, get_avatar() ) ? tr.loadid : trap_id();
    }
    const auto remembered = get_avatar().get_memorized_tile( map_local_to_abs( here, p ) );
    const auto id = trap_str_id( remembered.tile );
    return !remembered.tile.empty() && id.is_valid() ? id.id() : trap_id();
}

/// The ASCII wall mapping uses the same SEWN mask as the semantic tile channel.
auto wall_symbol( const uint8_t mask, const int fallback ) -> int
{
    const auto symbols = std::array{
        fallback, LINE_XOXO, LINE_OXOX, LINE_XXOO,
        LINE_XOXO, LINE_XOXO, LINE_OXXO, LINE_XXXO,
        LINE_OXOX, LINE_XOOX, LINE_OXOX, LINE_XXOX,
        LINE_OOXX, LINE_XOXX, LINE_OXXX, LINE_XXXX
    };
    return symbols[mask];
}

template<typename Id>
auto connection_mask( const Id &id, const std::array<Id, 4> &adjacent ) -> uint8_t
{
    auto mask = uint8_t{ 0 };
    auto group = 0;
    const auto grouped = id.obj().connects( group );
    for( const auto i : std::views::iota( 0, 4 ) ) {
        if( adjacent[i] && ( grouped ? adjacent[i].obj().connects_to( group ) : adjacent[i] == id ) ) {
            mask |= 1 << i;
        }
    }
    return mask;
}

struct cell_memory {
    tripoint_abs_ms position;
    int symbol = 0;
    memorized_terrain_tile terrain{};
    memorized_terrain_tile overlay{};
};

auto derive( const map &here, const cell_facts &facts ) -> cell_memory
{
    const auto &p = facts.position;
    if( facts.vehicle_only ) {
        auto result = cell_memory{
            .position = map_local_to_abs( here, p ),
            .symbol = get_avatar().get_memorized_symbol( map_local_to_abs( here, p ) ),
            .terrain = get_avatar().get_terrain_tile( map_local_to_abs( here, p ) ),
            .overlay = get_avatar().get_memorized_tile( map_local_to_abs( here, p ) )
        };
        if( facts.stationary_vehicle ) {
            result.overlay = facts.vehicle_tile;
            result.symbol = facts.vehicle_symbol;
        } else if( result.overlay.tile.starts_with( "vp_" ) ) {
            // Actor-aware movement clears a vehicle ghost, without reading the
            // unseen current ground or deriving new connections beneath it.
            result.overlay = mm_submap::default_tile;
            const auto historical = ter_str_id( result.terrain.tile );
            result.symbol = 0;
            if( !result.terrain.tile.empty() && historical.is_valid() ) {
                result.symbol = historical->symbol();
                if( historical->has_flag( TFLAG_AUTO_WALL_SYMBOL ) ) {
                    for( const auto mask : std::views::iota( 0, 16 ) ) {
                        const auto shape = orient( mask );
                        if( shape.subtile == result.terrain.subtile &&
                            shape.rotation == result.terrain.rotation ) {
                            result.symbol = wall_symbol( mask, result.symbol );
                            break;
                        }
                    }
                }
            }
        }
        return result;
    }
    auto terrain_neighbors = std::array<ter_id, 4> {};
    auto furniture_neighbors = std::array<furn_id, 4> {};
    auto trap_neighbors = std::array<trap_id, 4> {};
    for( const auto i : std::views::iota( 0, 4 ) ) {
        terrain_neighbors[i] = known_terrain( here, p + neighbors[i] );
        furniture_neighbors[i] = known_furniture( here, p + neighbors[i] );
        trap_neighbors[i] = known_trap( here, p + neighbors[i] );
    }
    auto result = cell_memory{ .position = map_local_to_abs( here, p ) };
    const auto terrain_mask = connection_mask( facts.terrain, terrain_neighbors );
    const auto terrain_orientation = orient( terrain_mask );
    result.symbol = facts.terrain->has_flag( TFLAG_AUTO_WALL_SYMBOL ) ?
                    wall_symbol( terrain_mask, facts.terrain->symbol() ) : facts.terrain->symbol();
    if( !facts.terrain->has_flag( TFLAG_NO_MEMORY ) &&
        !facts.terrain->has_flag( TFLAG_Z_TRANSPARENT ) ) {
        result.terrain = { .tile = facts.terrain.id().str(),
                           .subtile = terrain_orientation.subtile, .rotation = terrain_orientation.rotation
                         };
    }
    if( facts.furniture ) {
        auto furniture_orientation = orient( connection_mask( facts.furniture, furniture_neighbors ) );
        if( furniture_orientation.subtile == unconnected ) {
            auto walls = 0;
            auto workbenches = 0;
            for( const auto i : std::views::iota( 0, 4 ) ) {
                // Alignment mask is NESW, unlike connection masks.
                const auto bit = std::array{ 4, 2, 8, 1 } [i];
                const auto &ter = terrain_neighbors[i].obj();
                if( ter.has_flag( TFLAG_WALL ) || ter.has_flag( "WINDOW" ) ||
                    ter.has_flag( "DOOR" ) ) {
                    walls |= bit;
                }
                if( furniture_neighbors[i] && furniture_neighbors[i]->workbench ) {
                    workbenches |= bit;
                }
            }
            const auto use_workbench = facts.furniture->has_flag( "ALIGN_WORKBENCH" ) && workbenches;
            const auto alignment = use_workbench ? workbenches : walls;
            const auto rotations = std::array{ 0, 0, 1, 0, 2, 1, 1, 1, 3, 0, 0, 0, 3, 3, 2, 0 };
            furniture_orientation.rotation = ( rotations[alignment] + ( use_workbench ? 2 : 0 ) ) % 4;
        }
        result.symbol = facts.furniture->symbol();
        result.overlay = { .tile = facts.furniture.id().str(),
                           .subtile = furniture_orientation.subtile, .rotation = furniture_orientation.rotation
                         };
    }
    if( facts.trap ) {
        result.symbol = facts.trap->sym == '%' ? '*' : facts.trap->sym;
        if( facts.trap.id().str() != "tr_ledge" ) {
            auto mask = uint8_t{ 0 };
            for( const auto i : std::views::iota( 0, 4 ) ) {
                if( trap_neighbors[i] == facts.trap ) {
                    mask |= 1 << i;
                }
            }
            const auto tr_orientation = orient( mask );
            result.overlay = { .tile = facts.trap.id().str(),
                               .subtile = tr_orientation.subtile, .rotation = tr_orientation.rotation
                             };
        }
    }
    if( facts.field_symbol && ( facts.field_overrides || result.symbol == '.' ) ) {
        result.symbol = facts.field_symbol;
    }
    if( facts.stationary_vehicle ) {
        result.overlay = facts.vehicle_tile;
        result.symbol = facts.vehicle_symbol;
    }
    return result;
}

auto observe( const map &here, const tripoint_bub_ms &p ) -> cell_facts
{
    auto facts = cell_facts{ .position = p, .vehicle_only = !normally_visible( here, p ) };
    const auto &you = get_avatar();
    if( !facts.vehicle_only ) {
        facts.terrain = here.ter( p );
        facts.furniture = here.furn( p );
        const auto &tr = here.tr_at( p );
        if( tr.can_see( p, you ) ) {
            facts.trap = tr.loadid;
        }
        const auto &fields = here.field_at( p );
        if( fields.field_count() ) {
            const auto &id = fields.displayed_field_type();
            const auto sym = id->get_symbol();
            if( fields.find_field( id ) && sym != "&" && sym != "%" && !sym.empty() ) {
                facts.field_symbol = sym == "*" ? '*' : sym[0];
                facts.field_overrides = sym == "*" || id->priority > 1;
            }
        }
    }
    const auto vp = here.veh_at( p );
    if( vp ) {
        const auto &veh = vp->vehicle();
        facts.roof = p.z() < you.bub_pos().z();
        facts.stationary_vehicle = !veh.forward_velocity() && !veh.player_in_control( you );
        auto modifier = char{ 0 };
        const auto &id = veh.part_id_string( vp->part_index(), facts.roof, modifier );
        const auto direction = veh.part_display_direction( vp->part_index(), facts.roof );
        facts.vehicle_tile = { .tile = "vp_" + id.str(),
                               .subtile = modifier == 1 ? open_ : modifier == 2 ? broken : 0,
                               .rotation = static_cast<int>( std::round( to_degrees( direction ) ) )
                             };
        facts.vehicle_symbol = special_symbol( tileray( direction ).dir_symbol(
                veh.part_sym( vp->part_index(), facts.roof ) ) );
    }
    return facts;
}

} // namespace

auto cosmetic_variant() -> int
{
    static auto cosmetic = std::minstd_rand( 1 );
    return 1 + static_cast<int>( cosmetic() % 5 );
}

auto orient( const uint8_t connections ) -> orientation
{
    const auto subtiles = std::array{ unconnected, end_piece, end_piece, corner,
                                      end_piece, corner, edge, t_connection, end_piece,
                                      edge, corner, t_connection, corner, t_connection, t_connection, center };
    const auto rotations = std::array{ 0, 0, 1, 0, 3, 3, 1, 0, 2, 0, 1, 1, 2, 3, 2, 0 };
    return { .subtile = subtiles[connections & 15], .rotation = rotations[connections & 15] };
}

auto vehicle_known( const map &here, const vehicle &veh ) -> bool
{
    const auto &you = get_avatar();
    const auto occupied = here.veh_at( you.bub_pos() );
    if( you.in_vehicle && occupied && &occupied->vehicle() == &veh ) {
        return true;
    }
    // The native remote-control capability supplies vehicle telemetry, not
    // vision of the ground beneath it. Match its actor-held target, power and
    // radio range without remote_controlled()'s disconnect/message side effects.
    const auto target_value = you.get_value( "remote_controlling_vehicle" );
    if( target_value.empty() || ( !you.has_active_bionic( bionic_id( "bio_remote" ) ) &&
                                  !you.has_active_item_with_action( "REMOTEVEH" ) ) ) {
        return false;
    }
    auto stream = std::istringstream( target_value );
    auto target = tripoint_bub_ms{};
    if( !( stream >> target.x() >> target.y() >> target.z() ) || target != veh.bub_ms_location() ||
        veh.fuel_left( itype_id( "battery" ), true ) <= 0 ) {
        return false;
    }
    return std::ranges::any_of( veh.get_avail_parts( "REMOTE_CONTROLS" ),
    [&]( const auto & part ) { return rl_dist( you.bub_pos(), part.pos() ) <= 40; } );
}

auto visible_at( const map &here, const tripoint_bub_ms &p ) -> bool
{
    return normally_visible( here, p );
}

auto visible_cells( const map &here ) -> std::vector<tripoint_bub_ms>
{
    auto result = std::vector<tripoint_bub_ms> {};
    const auto &you = get_avatar();
    // A blind avatar sees only within clairvoyance, which needs the per-cell test.
    const auto blind = you.is_blind();
    if( blind && you.clairvoyance() == 0 ) { return result; }
    const auto &vision = here.get_visibility_variables_cache();
    // Classify each light level once instead of calling the map per cell.
    auto clear = std::array < bool, static_cast<size_t>( lit_level::BLANK ) + 1 > {};
    for( const auto level : std::views::iota( 0, static_cast<int>( clear.size() ) ) ) {
        clear[level] = here.get_visibility( static_cast<lit_level>( level ), vision ) == VIS_CLEAR;
    }
    for( const auto z : std::views::iota( -OVERMAP_DEPTH, OVERMAP_HEIGHT + 1 ) ) {
        const auto &cache = here.access_cache( z );
        if( cache.visibility_cache.size() < static_cast<size_t>( cache.cache_x * cache.cache_y ) ) { continue; }
        for( const auto x : std::views::iota( 0, cache.cache_x ) ) {
            for( const auto y : std::views::iota( 0, cache.cache_y ) ) {
                if( !clear[static_cast<size_t>( cache.visibility_cache[cache.idx( x, y )] )] ) {
                    continue;
                }
                const auto p = tripoint_bub_ms( x, y, z );
                if( !blind || normally_visible( here, p ) ) { result.push_back( p ); }
            }
        }
    }
    return result;
}

auto detailed_at( const map &here, const tripoint_bub_ms &p ) -> bool
{
    if( !here.inbounds( p ) ) {
        return false;
    }
    if( normally_visible( here, p ) ) {
        return true;
    }
    const auto vp = here.veh_at( p );
    if( !vp ) {
        return false;
    }
    if( vehicle_known( here, vp->vehicle() ) ) {
        return true;
    }
    // A vehicle floor can occlude its own roof in 3-D FoV. Only a clear column of
    // exposed air above it grants the exterior surface, not the ground beneath it.
    if( p.z() >= get_avatar().bub_pos().z() ) {
        return false;
    }
    for( const auto z : std::views::iota( p.z() + 1, get_avatar().bub_pos().z() + 1 ) ) {
        const auto above = tripoint_bub_ms( p.xy(), z );
        if( !here.has_flag( TFLAG_NO_FLOOR, above ) || !normally_visible( here, above ) ) {
            return false;
        }
    }
    return true;
}

auto reset() -> void { state = acquisition_state{}; }

namespace
{
auto presentation_depth = 0;
} // namespace

presentation_scope::presentation_scope() { ++presentation_depth; }
presentation_scope::~presentation_scope() { --presentation_depth; }
auto presenting() -> bool { return presentation_depth > 0; }

auto invalidate_visibility( const map &here ) -> void
{
    if( state.owner == &here ) {
        state.full_pending = true;
    }
}

auto invalidate_cell( const map &here, const tripoint_bub_ms &p ) -> void
{
    if( state.owner != &here || state.committing || !here.inbounds( p ) ) {
        return;
    }
    state.dirty.insert( p );
    state.pending.insert( p );
    for( const auto &delta : neighbors ) {
        if( here.inbounds( p + delta ) ) {
            state.dirty.insert( p + delta );
            state.pending.insert( p + delta );
        }
    }
}

auto memory_changed( const tripoint_abs_ms &p ) -> void
{
    if( g ) {
        invalidate_cell( get_map(), abs_to_map_local( get_map(), p ) );
    }
}

auto acquire() -> acquisition_counts
{
    auto counts = acquisition_counts{};
    auto &here = get_map();
    auto &you = get_avatar();
    const auto zlev = you.bub_pos().z();
    if( state.owner != &here || state.origin != here.get_abs_sub() ||
        state.dimension != here.get_bound_dimension() || state.size != here.getmapsize() ) {
        reset();
        state.owner = &here;
        state.origin = here.get_abs_sub();
        state.dimension = here.get_bound_dimension();
        state.size = here.getmapsize();
    }
    you.recalc_sight_limits();
    g->reset_light_level();
    const auto vision = here.make_visibility_variables( zlev );
    // Detect changed actor knowledge before the CPU cached-light shortcut. Unchanged
    // inputs reuse native caches; camera offset, zoom and redraw are not inputs.
    if( state.position != you.abs_pos() || !same_vision( state.vision, vision ) ||
        state.blind != you.is_blind() || state.threshold != g_visible_threshold ) {
        here.invalidate_visibility_caches();
    }
    // Native sparse dynamic-light detection must run even with a clean visibility flag.
    ++counts.cache_checks;
    here.build_map_cache( zlev, false );
    if( here.visibility_caches_dirty() ) {
        ++counts.visibility_updates;
        here.update_visibility_cache( zlev );
    }
    state.position = you.abs_pos();
    state.vision = here.get_visibility_variables_cache();
    state.blind = you.is_blind();
    state.threshold = g_visible_threshold;

    auto fields = decltype( state.fields ) {};
    auto traps = decltype( state.traps ) {};
    for( const auto &view : here.active_submap_views() ) {
        ++counts.submaps_checked;
        const auto &sm = view.get_submap();
        for( const auto &local : sm.trap_cache ) {
            ++counts.traps_checked;
            const auto trap = sm.get_effective_trap( local );
            if( trap != tr_null ) {
                traps.emplace( abs_to_map_local( here, project_combine( view.abs_pos(), local ) ), trap );
            }
        }
        for( const auto &local : sm.field_cache ) {
            ++counts.fields_checked;
            const auto p = abs_to_map_local( here, project_combine( view.abs_pos(), local ) );
            const auto &field = sm.get_field( local );
            if( field.field_count() ) {
                fields.emplace( p, field_appearance{ .field = field.displayed_field_type(),
                                                     .terrain = sm.get_ter( local ), .furniture = sm.get_furn( local ) } );
            }
        }
    }
    update_sparse( here, state.fields, std::move( fields ) );
    update_sparse( here, state.traps, std::move( traps ) );
    auto vehicles = decltype( state.vehicles ) {};
    for( const auto &wrapped : here.get_vehicles() ) {
        for( const auto &abs : wrapped.v->get_points() ) {
            ++counts.vehicle_points_checked;
            const auto p = abs_to_map_local( here, abs );
            if( !here.inbounds( p ) ) {
                continue;
            }
            const auto facts = observe( here, p );
            vehicles.emplace( p, vehicle_appearance{ .tile = facts.vehicle_tile,
                              .symbol = facts.vehicle_symbol, .stationary = facts.stationary_vehicle,
                              .detailed = detailed_at( here, p ) } );
        }
    }
    update_sparse( here, state.vehicles, std::move( vehicles ) );
    if( !state.full_pending && state.dirty.empty() ) {
        return counts;
    }
    const auto &cache = here.access_cache( zlev );
    you.prepare_map_memory_region( map_local_to_abs( here, tripoint_bub_ms( 0, 0, zlev ) ),
                                   map_local_to_abs( here, tripoint_bub_ms( cache.cache_x - 1, cache.cache_y - 1, zlev ) ) );
    auto facts = std::vector<cell_facts> {};
    const auto acquire_cell = [&]( const auto & p ) {
        ++counts.candidates;
        if( detailed_at( here, p ) ) {
            facts.push_back( observe( here, p ) );
            state.pending.erase( p );
        }
    };
    if( state.full_pending ) {
        ++counts.full_rescans;
        auto seen = std::vector<int>( static_cast<size_t>( here.getmapsize() ) * here.getmapsize(), 0 );
        for( const auto y : std::views::iota( 0, cache.cache_y ) ) {
            for( const auto x : std::views::iota( 0, cache.cache_x ) ) {
                const auto light = cache.visibility_cache[cache.idx( x, y )];
                seen[( x / SEEX ) * here.getmapsize() + y / SEEY] +=
                    light == lit_level::BRIGHT || light == lit_level::LIT;
                // Follow exposed surfaces, not the camera and not every clear z-level.
                for( const auto z : std::views::iota( -OVERMAP_DEPTH, zlev + 1 ) | std::views::reverse ) {
                    const auto p = tripoint_bub_ms( x, y, z );
                    acquire_cell( p );
                    if( here.dont_draw_lower_floor( p ) || here.veh_at( p ) ) {
                        break;
                    }
                }
            }
        }
        for( const auto y : std::views::iota( 0, here.getmapsize() ) ) {
            for( const auto x : std::views::iota( 0, here.getmapsize() ) ) {
                if( seen[x * here.getmapsize() + y] > 36 ) {
                    const auto sm = tripoint_bub_sm( x, y, zlev );
                    const auto omt = project_to<coords::omt>( map_local_to_abs( here, sm ) );
                    get_overmapbuffer( here.get_bound_dimension() ).set_seen( tripoint_abs_omt( omt.xy(), 0 ), true );
                }
            }
        }
    } else {
        for( const auto &p : state.dirty ) {
            // Only exposed surfaces in this column are candidates, even for dirty lower cells.
            if( p.z() > zlev ) {
                continue;
            }
            auto exposed = true;
            for( const auto z : std::views::iota( p.z() + 1, zlev + 1 ) ) {
                const auto above = tripoint_bub_ms( p.xy(), z );
                if( here.dont_draw_lower_floor( above ) || here.veh_at( above ) ) {
                    exposed = false;
                    break;
                }
            }
            if( exposed ) {
                acquire_cell( p );
            }
        }
    }
    // Derive against the completed facts and previous memory, before committing anything.
    auto memories = std::vector<cell_memory> {};
    memories.reserve( facts.size() );
    for( const auto &cell : facts ) {
        memories.push_back( derive( here, cell ) );
    }
    state.committing = true;
    const auto finish_commit = on_out_of_scope( []() { state.committing = false; } );
    for( const auto &cell : memories ) {
        if( you.get_terrain_tile( cell.position ) != cell.terrain ) {
            you.memorize_terrain_tile( cell.position, cell.terrain.tile, cell.terrain.subtile,
                                       cell.terrain.rotation );
            ++counts.memory_writes;
        }
        if( you.get_memorized_tile( cell.position ) != cell.overlay ) {
            you.memorize_tile( cell.position, cell.overlay.tile, cell.overlay.subtile, cell.overlay.rotation );
            ++counts.memory_writes;
        }
        if( you.get_memorized_symbol( cell.position ) != cell.symbol ) {
            you.memorize_symbol( cell.position, cell.symbol );
            ++counts.memory_writes;
        }
    }
    for( const auto &cell : facts ) {
        here.check_and_set_seen_cache( cell.position );
    }
    for( const auto z : std::views::iota( -OVERMAP_DEPTH, OVERMAP_HEIGHT + 1 ) ) {
        // Hidden dirty facts remain in pending, not in the old renderer work queue.
        here.mark_memory_seen_cache_dirty_all_clean( z );
    }
    state.full_pending = false;
    state.dirty.clear();
    counts.acquired = memories.size();
    return counts;
}

auto contact( const tripoint_bub_ms &p ) -> void
{
    auto &here = get_map();
    auto &you = get_avatar();
    if( !here.inbounds( p ) ) {
        return;
    }
    const auto abs = map_local_to_abs( here, p );
    const auto vp = here.veh_at( p );
    if( vp ) {
        const auto &veh = vp->vehicle();
        auto modifier = char{ 0 };
        const auto &id = veh.part_id_string( vp->part_index(), false, modifier );
        you.memorize_tile( abs, "vp_" + id.str(), modifier == 1 ? open_ : modifier == 2 ? broken : 0, 0 );
        you.memorize_symbol( abs, special_symbol( veh.part_sym( vp->part_index() ) ) );
    } else if( here.has_furn( p ) ) {
        you.memorize_tile( abs, here.furn( p ).id().str(), 0, 0 );
        you.memorize_symbol( abs, here.furn( p )->symbol() );
    } else {
        const auto &ter = here.ter( p );
        // Classify recorded producer IDs, not hidden world traps. Unknown or
        // cross-category ambiguous descriptors are not evidence of contradiction.
        const auto overlay = you.get_memorized_tile( abs ).tile;
        const auto furniture = furn_str_id( overlay ).is_valid() && furn_str_id( overlay ).id() != f_null;
        const auto vehicle_id = overlay.starts_with( "vp_" ) ? vpart_id( overlay.substr(
                                    3 ) ) : vpart_id::NULL_ID();
        const auto vehicle = vehicle_id.is_valid() && vehicle_id != vpart_id::NULL_ID();
        const auto terrain = ter_str_id( overlay ).is_valid() && ter_str_id( overlay ).id() != t_null;
        const auto trap = trap_str_id( overlay ).is_valid();
        if( !trap && furniture + vehicle + terrain == 1 &&
            ( furniture || vehicle || ter_str_id( overlay ).id() != ter ) ) {
            you.clear_memorized_overlay( abs );
        }
        you.memorize_terrain_tile( abs, ter.id().str(), 0, 0 );
        you.memorize_symbol( abs, ter->symbol() );
    }
}

} // namespace map_perception
