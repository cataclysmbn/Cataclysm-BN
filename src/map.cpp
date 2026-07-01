#include "map.h"

#include "active_tile_data.h"
#include "faction.h"
#include "mapdata.h"
#include "mapgen_async.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <climits>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <ranges>
#include <limits>
#include <mutex>
#include <shared_mutex>
#include <optional>
#include <ostream>
#include <queue>
#include <type_traits>
#include <unordered_map>
#include <variant>
#include <vector>

#include "active_item_cache.h"
#include "ammo.h"
#include "ammo_effect.h"
#include "artifact.h"
#include "avatar.h"
#include "bodypart.h"
#include "calendar.h"
#include "catalua_hooks.h"
#include "catalua_sol.h"
#include "cata_cartesian_product.h"
#include "cata_utility.h"
#include "cached_options.h"
#include "character.h"
#include "character_id.h"
#include "clzones.h"
#include "color.h"
#include "construction.h"
#include "coordinates.h"
#include "creature.h"
#include "cursesdef.h"
#include "damage.h"
#include "debug.h"
#include "detached_ptr.h"
#include "distribution_grid.h"
#include "drawing_primitives.h"
#include "enums.h"
#include "event.h"
#include "event_bus.h"
#include "explosion.h"
#include "explosion_queue.h"
#include "field.h"
#include "field_type.h"
#include "flag.h"
#include "flat_set.h"
#include "fragment_cloud.h"
#include "fluid_grid.h"
#include "fungal_effects.h"
#include "game.h"
#include "game_constants.h"
#include "harvest.h"
#include "iexamine.h"
#include "input.h"
#include "int_id.h"
#include "item.h"
#include "item_category.h"
#include "item_contents.h"
#include "item_factory.h"
#include "item_group.h"
#include "itype.h"
#include "iuse.h"
#include "iuse_actor.h"
#include "lightmap.h"
#include "line.h"
#include "map_functions.h"
#include "map_iterator.h"
#include "map_memory.h"
#include "map_selector.h"
#include "mapbuffer.h"
#include "map_feature_descriptions.h"
#include "math_defines.h"
#include "memory_fast.h"
#include "messages.h"
#include "mission.h"
#include "mongroup.h"
#include "monster.h"
#include "morale_types.h"
#include "mtype.h"
#include "npc.h"
#include "options.h"
#include "output.h"
#include "overmapbuffer.h"
#include "legacy_pathfinding.h"
#include "player.h"
#include "point.h"
#include "point_float.h"
#include "projectile.h"
#include "profile.h"
#include "rng.h"
#include "rot.h"
#include "safe_reference.h"
#include "scent_map.h"
#include "sounds.h"
#include "string_formatter.h"
#include "string_id.h"
#include "submap.h"
#include "submap_load_manager.h"
#include "thread_pool.h"
#include "tileray.h"
#include "timed_event.h"
#include "translations.h"
#include "trap.h"
#include "ui_manager.h"
#include "value_ptr.h"
#include "veh_type.h"
#include "vehicle.h"
#include "vehicle_part.h"
#include "visitable.h"
#include "vpart_position.h"
#include "vpart_range.h"
#include "weather.h"
#include "weighted_list.h"

#if defined( CATA_SDL )
#include "compute/compute_backend.h"
#include "compute/gpu_lm.h"
#include "compute/gpu_platform.h"
#endif

struct ammo_effect;
using ammo_effect_str_id = string_id<ammo_effect>;

static const ammo_effect_str_id ammo_effect_INCENDIARY( "INCENDIARY" );
static const ammo_effect_str_id ammo_effect_LASER( "LASER" );
static const ammo_effect_str_id ammo_effect_LIGHTNING( "LIGHTNING" );
static const ammo_effect_str_id ammo_effect_NO_PENETRATE_OBSTACLES( "NO_PENETRATE_OBSTACLES" );
static const ammo_effect_str_id ammo_effect_PLASMA( "PLASMA" );

static const itype_id itype_autoclave( "autoclave" );
static const itype_id itype_battery( "battery" );
static const itype_id itype_chemistry_set( "chemistry_set" );
static const itype_id itype_dehydrator( "dehydrator" );
static const itype_id itype_electrolysis_kit( "electrolysis_kit" );
static const itype_id itype_food_processor( "food_processor" );
static const itype_id itype_forge( "forge" );
static const itype_id itype_hotplate( "hotplate" );
static const itype_id itype_kiln( "kiln" );
static const itype_id itype_press( "press" );
static const itype_id itype_soldering_iron( "soldering_iron" );
static const itype_id itype_vac_sealer( "vac_sealer" );
static const itype_id itype_welder( "welder" );
static const itype_id itype_butchery( "fake_adv_butchery" );

static const mtype_id mon_zombie( "mon_zombie" );

static const skill_id skill_traps( "traps" );

static const efftype_id effect_boomered( "boomered" );
static const efftype_id effect_crushed( "crushed" );
static const efftype_id effect_haslight( "haslight" );
static const efftype_id effect_onfire( "onfire" );

static const ter_str_id t_rock_floor_no_roof( "t_rock_floor_no_roof" );

static const std::string str_DOOR_LOCKING( "DOOR_LOCKING" );
static const std::string str_OPENCLOSE_INSIDE( "OPENCLOSE_INSIDE" );

namespace
{

auto horde_should_avoid_vehicle_tile( const map &here, const tripoint_bub_ms &p,
                                      const mongroup &group ) -> bool
{
    if( !group.horde ) {
        return false;
    }

    const auto vp = here.veh_at( p );
    if( !vp ) {
        return false;
    }

    const auto &veh = vp->vehicle();
    return veh.is_owned_by( get_avatar() );
}

auto quantized_light_signature_value( const float value ) -> int
{
    return static_cast<int>( std::lround( value * 4.0f ) );
}

auto quantized_solar_signature_value( const float value ) -> int
{
    return static_cast<int>( std::lround( value * 8.0f ) );
}

auto quantized_angle_signature_value( const units::angle angle ) -> int
{
    return static_cast<int>( std::lround( units::to_degrees( angle ) * 10.0 ) );
}

void hash_light_item( std::size_t &seed, const tripoint_bub_ms &pos, const item &itm )
{
    auto luminance = 0.0f;
    auto width = 0_degrees;
    auto direction = 0_degrees;
    if( !itm.getlight( luminance, width, direction ) ) {
        return;
    }
    cata::hash_combine( seed, pos );
    cata::hash_combine( seed, quantized_light_signature_value( luminance ) );
    cata::hash_combine( seed, quantized_angle_signature_value( width ) );
    cata::hash_combine( seed, quantized_angle_signature_value( direction ) );
}

void hash_character_light_state( std::size_t &seed, const Character &who )
{
    const auto active_luminance = who.active_light();
    const auto has_fire_light = who.has_effect( effect_onfire );
    const auto has_cached_light = who.has_effect( effect_haslight );
    if( active_luminance <= LIGHT_AMBIENT_LOW && !has_fire_light && !has_cached_light ) {
        return;
    }
    cata::hash_combine( seed, who.bub_pos() );
    cata::hash_combine( seed, quantized_light_signature_value( active_luminance ) );
    cata::hash_combine( seed, has_fire_light );
    cata::hash_combine( seed, has_cached_light );
}

} // namespace

#define dbg(x) DebugLog((x),DC::Map)

static location_vector<item> nulitems( new
                                       fake_item_location() );       // Returned when &i_at() is asked for an OOB value
static field              nulfield;          // Returned when &field_at() is asked for an OOB value
static level_cache        nullcache;         // Dummy cache for z-levels outside bounds

bool disable_mapgen = false;

// Thread-local context for get_map().  Null means "use the global g->m."
// Worker threads never push a context, so they always fall through to g->m.
static thread_local map *tl_map_context = nullptr;

map &get_map()
{
    return tl_map_context ? *tl_map_context : g->m;
}

static auto checked_reality_bubble_size( const int reality_bubble_size ) -> int
{
    return std::clamp( reality_bubble_size, 0, REALITY_BUBBLE_SIZE_MAX );
}

auto reality_bubble_origin_from_player( const tripoint_abs_ms &player_pos,
                                        const int reality_bubble_size ) -> tripoint_abs_sm
{
    const auto player_sm = project_to<coords::sm>( player_pos );
    const auto half_mapsize = checked_reality_bubble_size( reality_bubble_size ) + 1;
    return player_sm - tripoint_rel_sm( half_mapsize, half_mapsize, 0 );
}

auto reality_bubble_center_from_origin( const tripoint_abs_sm &origin,
                                        const int reality_bubble_size ) -> tripoint_abs_sm
{
    const auto half_mapsize = checked_reality_bubble_size( reality_bubble_size ) + 1;
    return origin + tripoint_rel_sm( half_mapsize, half_mapsize, 0 );
}

auto player_reality_bubble_origin() -> tripoint_abs_sm
{
    return reality_bubble_origin_from_player( get_avatar().abs_pos(), g_reality_bubble_size );
}

auto is_in_reality_bubble_bounds( const tripoint_bub_sm &p ) -> bool
{
    const auto bubble_size = checked_reality_bubble_size( g_reality_bubble_size );
    const auto map_size = 2 * bubble_size + 3;
    return p.z() >= -OVERMAP_DEPTH && p.z() <= OVERMAP_HEIGHT &&
           p.x() >= 0 && p.x() < map_size &&
           p.y() >= 0 && p.y() < map_size;
}

auto is_in_reality_bubble_bounds( const tripoint_bub_ms &p ) -> bool
{
    return is_in_reality_bubble_bounds( project_to<coords::sm>( p ) );
}

auto bub_to_abs( const tripoint_bub_ms &p ) -> tripoint_abs_ms
{
    const auto origin = project_to<coords::ms>( player_reality_bubble_origin() );
    return ( p + origin.xy().raw() ).reinterpret_as<tripoint_abs_ms>();
}

auto abs_to_bub( const tripoint_abs_ms &p ) -> tripoint_bub_ms
{
    const auto origin = project_to<coords::ms>( player_reality_bubble_origin() );
    return ( p - origin.xy() ).reinterpret_as<tripoint_bub_ms>();
}

auto bub_to_abs( const tripoint_bub_sm &p ) -> tripoint_abs_sm
{
    const auto origin = player_reality_bubble_origin();
    return ( p + origin.xy().raw() ).reinterpret_as<tripoint_abs_sm>();
}

auto abs_to_bub( const tripoint_abs_sm &p ) -> tripoint_bub_sm
{
    const auto origin = player_reality_bubble_origin();
    return ( p - origin.xy() ).reinterpret_as<tripoint_bub_sm>();
}

auto bub_to_abs( const point_bub_ms &p ) -> point_abs_ms
{
    const auto origin = project_to<coords::ms>( player_reality_bubble_origin() ).xy();
    return origin + p.raw();
}

auto abs_to_bub( const point_abs_ms &p ) -> point_bub_ms
{
    const auto origin = project_to<coords::ms>( player_reality_bubble_origin() ).xy();
    return ( p - origin ).reinterpret_as<point_bub_ms>();
}

auto bub_to_abs( const point_bub_sm &p ) -> point_abs_sm
{
    const auto origin = player_reality_bubble_origin().xy();
    return origin + p.raw();
}

auto abs_to_bub( const point_abs_sm &p ) -> point_bub_sm
{
    const auto origin = player_reality_bubble_origin().xy();
    return ( p - origin ).reinterpret_as<point_bub_sm>();
}

auto map_local_to_abs( const map &m, const tripoint_bub_ms &local ) -> tripoint_abs_ms
{
    const auto origin = project_to<coords::ms>( m.get_abs_sub() );
    return ( local + origin.raw() ).reinterpret_as<tripoint_abs_ms>();
}

auto abs_to_map_local( const map &m, const tripoint_abs_ms &abs ) -> tripoint_bub_ms
{
    const auto origin = project_to<coords::ms>( m.get_abs_sub() );
    return ( abs - origin ).reinterpret_as<tripoint_bub_ms>();
}

auto map_local_to_abs( const map &m, const tripoint_bub_sm &local ) -> tripoint_abs_sm
{
    const auto origin = m.get_abs_sub();
    return ( local + origin.raw() ).reinterpret_as<tripoint_abs_sm>();
}

auto abs_to_map_local( const map &m, const tripoint_abs_sm &abs ) -> tripoint_bub_sm
{
    const auto origin = m.get_abs_sub();
    return ( abs - origin ).reinterpret_as<tripoint_bub_sm>();
}

auto map_local_to_abs( const map &m, const point_bub_ms &local ) -> point_abs_ms
{
    const auto origin = project_to<coords::ms>( m.get_abs_sub() );
    return origin + local.raw();
}

auto abs_to_map_local( const map &m, const point_abs_ms &abs ) -> point_bub_ms
{
    const auto origin = project_to<coords::ms>( m.get_abs_sub() );
    return ( abs - origin ).reinterpret_as<point_bub_ms>();
}

auto map_local_to_abs( const map &m, const point_bub_sm &local ) -> point_abs_sm
{
    return m.get_abs_sub() + local.raw();
}

auto abs_to_map_local( const map &m, const point_abs_sm &abs ) -> point_bub_sm
{
    return ( abs - m.get_abs_sub() ).reinterpret_as<point_bub_sm>();
}

scoped_map_context::scoped_map_context( map &m ) noexcept
    : prev_( tl_map_context )
{
    tl_map_context = &m;
}

scoped_map_context::~scoped_map_context() noexcept
{
    tl_map_context = prev_;
}

auto resident_item_lookup() -> mapbuffer_lookup_options
{
    return {
        .mode = mapbuffer_lookup_mode::resident_only,
    };
}

auto can_delegate_item_mutation_to_mapbuffer( const map &m, const tripoint_abs_ms &p,
        const submap *sm ) -> bool
{
    if( sm == nullptr || g == nullptr || &get_map() != &m ) {
        return false;
    }

    return MAPBUFFER_REGISTRY.get( m.get_bound_dimension() ).lookup_submap_in_memory(
               project_to<coords::sm>( p ) ) == sm;
}

auto map_stack::local_location() const -> tripoint_bub_ms
{
    if( local_origin != nullptr ) {
        return abs_to_map_local( *local_origin, location );
    }
    return abs_to_bub( location );
}

map_stack::iterator map_stack::erase( map_stack::const_iterator it, detached_ptr<item> *out )
{
    if( myorigin != nullptr ) {
        return myorigin->erase_item( location, {
            .it = std::move( it ),
            .out = out,
            .lookup = resident_item_lookup(),
        } );
    }
    if( local_origin != nullptr ) {
        return local_origin->i_rem( local_location(), std::move( it ), out );
    }
    return location_vector<item>::iterator();
}

void map_stack::insert( detached_ptr<item> &&newitem )
{
    if( local_origin != nullptr ) {
        local_origin->add_item_or_charges( local_location(), std::move( newitem ) );
        return;
    }
    if( myorigin != nullptr ) {
        myorigin->add_item_or_charges( location, std::move( newitem ), {
            .lookup = resident_item_lookup(),
        } );
    }
}
detached_ptr<item> map_stack::remove( item *to_remove )
{
    if( myorigin != nullptr ) {
        return myorigin->remove_item( location, to_remove, resident_item_lookup() );
    }
    if( local_origin != nullptr ) {
        return local_origin->i_rem( local_location(), to_remove );
    }
    return detached_ptr<item>();
}

auto map_stack::clear() -> std::vector<detached_ptr<item>>
{
    if( myorigin != nullptr ) {
        return myorigin->clear_items( location, resident_item_lookup() );
    }
    if( local_origin != nullptr ) {
        return local_origin->i_clear( local_location() );
    }
    return {};
}

units::volume map_stack::max_volume() const
{
    if( myorigin != nullptr ) {
        const auto furniture = myorigin->get_furn( location, resident_item_lookup() );
        if( furniture && *furniture != f_null ) {
            return furniture->obj().max_volume;
        }
        const auto terrain = myorigin->get_ter( location, resident_item_lookup() );
        if( terrain ) {
            return terrain->obj().max_volume;
        }
        return 0_ml;
    }
    const auto local = local_location();
    if( local_origin != nullptr && local_origin->has_furn( local ) ) {
        return local_origin->furn( local ).obj().max_volume;
    }
    if( local_origin != nullptr ) {
        return local_origin->ter( local ).obj().max_volume;
    }
    return 0_ml;
}

// Map class methods.

map::map( int mapsize )
{
    my_MAPSIZE = mapsize;

    for( auto &ptr : caches ) {
        ptr = std::make_unique<level_cache>( SEEX * mapsize, SEEY * mapsize );
    }

    dbg( DL::Info ) << "map::map(): my_MAPSIZE: " << my_MAPSIZE;
    skew_vision_cache_mutex = std::make_unique<std::shared_mutex>();
    skew_vision_cache.resize( vision_cache_slots );
}

map::~map() = default;
map &map::operator=( map && )  noexcept = default;

auto map::resize( int new_mapsize ) -> void
{
    my_MAPSIZE = new_mapsize;
    for( auto &ptr : caches ) {
        ptr = std::make_unique<level_cache>( SEEX * new_mapsize, SEEY * new_mapsize );
    }
    refresh_active_submap_view();
    funnel_locations_.clear();
    dbg( DL::Info ) << "map::resize(): my_MAPSIZE: " << my_MAPSIZE;
}

auto map::bind_dimension( const dimension_id &dim ) -> void
{
    bound_dimension_ = dim;
    refresh_active_submap_view();
}

auto map::refresh_active_submap_view() -> void
{
    active_submaps_ = mapbuffer_bounds_view( get_mapbuffer(), abs_sub,
    abs_sub + point_rel_sm( my_MAPSIZE, my_MAPSIZE ), {
        .mode = mapbuffer_lookup_mode::resident_only,
    } );
}

auto map::update_active_load_region( const point_abs_sm &begin,
                                     const point_abs_sm &end ) -> void
{
    if( !active_load_region_ ) {
        active_load_region_ = mapbuffer_load_region( {
            .buffer = get_mapbuffer(),
            .source = load_request_source::reality_bubble,
            .begin = begin,
            .end = end,
        } );
        return;
    }
    active_load_region_.update( begin, end );
}

auto map::release_active_load_region() -> void
{
    active_load_region_.release();
}

auto map::validate_active_submap_view_complete( const char *context ) const -> void
{
    if( active_submaps_.is_complete() ) {
        return;
    }

    for( const auto p : bubble_submaps() ) {
        const auto local = point_rel_sm( p.x(), p.y() );
        if( active_submaps_.get_submap_view( local, p.z() ) ) {
            continue;
        }
        const auto missing = map_local_to_abs( *this, p );
        debugmsg( "map active bounds view incomplete after %s; missing submap %s in dimension '%s'",
                  context, missing.to_string(), bound_dimension_.c_str() );
        return;
    }

    debugmsg( "map active bounds view incomplete after %s, but no missing active submap was found",
              context );
}

bool map::contains_abs_sm( const tripoint_abs_sm &p ) const
{
    if( p.x() < abs_sub.x() || p.x() >= abs_sub.x() + my_MAPSIZE ) {
        return false;
    }
    if( p.y() < abs_sub.y() || p.y() >= abs_sub.y() + my_MAPSIZE ) {
        return false;
    }
    return p.z() >= -OVERMAP_DEPTH && p.z() <= OVERMAP_HEIGHT;
}

void map::on_submap_loaded( const tripoint_abs_sm &p, const dimension_id &dim_id )
{
    if( dim_id != bound_dimension_ ) {
        return;
    }

    // Submap lookup shared by vehicle fixup, active-item tracking, and cache update.
    submap *sm = MAPBUFFER_REGISTRY.get( dim_id ).lookup_submap_in_memory( p );

    if( sm != nullptr && !sm->vehicles.empty() ) {
        MAPBUFFER_REGISTRY.get( dim_id ).refresh_vehicle_registry_for_submap( p, {
            .mode = mapbuffer_lookup_mode::resident_only,
        } );
    }

    get_mapbuffer().refresh_active_item_submap_index( p, resident_item_lookup() );

    // Register any funnel traps so fill_water_collectors can skip the mapbuffer scan.
    if( sm != nullptr && !sm->trap_cache.empty() ) {
        std::ranges::for_each( sm->trap_cache, [&]( const point_sm_ms & lp ) {
            if( sm->get_trap( lp ).obj().is_funnel() ) {
                funnel_locations_.emplace_back( p, lp );
            }
        } );
    }

}

void map::on_submap_unloaded( const tripoint_abs_sm &pos, const dimension_id &dim_id )
{
    if( dim_id != bound_dimension_ ) {
        return;
    }

    // Stamp the departure time so actualize() computes the correct time-since-simulated
    // on the next load.  Only meaningful for submaps that were actually simulated;
    // border-preloaded submaps that were never in the simulation set should not be
    // touched (their last_touched reflects their real generate-or-load time).
    if( submap_loader.is_in_simulated_set( dim_id, pos ) ) {
        submap *sm = MAPBUFFER_REGISTRY.get( dim_id ).lookup_submap_in_memory( pos );
        if( sm ) {
            sm->last_touched = calendar::turn;
        }
    }

    // Stop tracking active items for this submap.
    get_mapbuffer().forget_active_item_submap_index( pos );

    // Remove any funnel locations belonging to this submap.
    std::erase_if( funnel_locations_, [&]( const auto & e ) {
        return e.first == pos;
    } );

}

void map::set_transparency_cache_dirty( const int zlev )
{
    if( inbounds_z( zlev ) ) {
        get_cache( zlev ).transparency_cache_dirty.set();
        // If we are invalidating the entire transparency cache for this zlevel, the sound absorption cache will also be invalidated.
        get_cache( zlev ).absorption_cache_dirty.set();
        get_mapbuffer().mark_submap_caches_dirty( {
            .begin = abs_sub,
            .end = abs_sub + point_rel_sm( my_MAPSIZE, my_MAPSIZE ),
            .zlev = zlev,
            .transparency = true,
        } );
    }
}

void map::set_seen_cache_dirty( const tripoint_bub_ms &change_location )
{
    if( inbounds( change_location ) ) {
        level_cache &cache = get_cache( change_location.z() );
        if( cache.seen_cache_dirty ) {
            return;
        }
#if defined( CATA_SDL )
        cache.seen_cache_dirty = true;
#else
        const int ci = cache.idx( change_location.x(), change_location.y() );
        if( cache.seen_cache[ci] != 0.0 || cache.camera_cache[ci] != 0.0 ) {
            cache.seen_cache_dirty = true;
        }
#endif
    }
}

void map::set_outside_cache_dirty( const int zlev )
{
    if( inbounds_z( zlev ) ) {
        level_cache &ch = get_cache( zlev );
        ch.outside_cache_dirty.set();
        get_mapbuffer().mark_submap_caches_dirty( {
            .begin = abs_sub,
            .end = abs_sub + point_rel_sm( my_MAPSIZE, my_MAPSIZE ),
            .zlev = zlev,
            .outside = true,
        } );
    }
}

void map::set_outside_cache_dirty( const tripoint_bub_ms &p )
{
    if( !inbounds( p ) ) {
        return;
    }
    level_cache &ch = get_cache( p.z() );
    const auto proj = project_remain<coords::sm>( p );
    const auto smp = proj.quotient_tripoint;
    const auto l = proj.remainder;

    // Helper: mark one submap grid cell dirty in both the bitset and the submap flag.
    auto mark = [&]( const tripoint_bub_sm & p ) {
        if( p.x() < 0 || p.y() < 0 || p.x() >= my_MAPSIZE || p.y() >= my_MAPSIZE ) {
            return;
        }
        ch.outside_cache_dirty.set( static_cast<size_t>( ch.bidx( p.x(), p.y() ) ) );
        const auto abs_sm = map_local_to_abs( *this, tripoint_bub_sm{ p.x(), p.y(), p.z() } );
        get_mapbuffer().mark_submap_caches_dirty( {
            .begin = abs_sm.xy(),
            .end = abs_sm.xy() + point_rel_sm( 1, 1 ),
            .zlev = abs_sm.z(),
            .outside = true,
        } );
    };

    // Always mark the tile's own submap.
    mark( smp );

    // rebuild_outside_cache checks a 3×3 tile neighbourhood, so a tile on a
    // submap boundary can affect tiles in the adjacent submap.
    const bool on_left   = ( l.x() == 0 );
    const bool on_right  = ( l.x() == SEEX - 1 );
    const bool on_top    = ( l.y() == 0 );
    const bool on_bottom = ( l.y() == SEEY - 1 );

    if( on_left )   { mark( smp + point_rel_sm::west() ); }
    if( on_right )  { mark( smp + point_rel_sm::east() ); }
    if( on_top )    { mark( smp + point_rel_sm::north() ); }
    if( on_bottom ) { mark( smp + point_rel_sm::south() ); }

    // Corner neighbours when on both x and y boundaries.
    if( on_left  && on_top )    { mark( smp + point_rel_sm::north_west() ); }
    if( on_right && on_top )    { mark( smp + point_rel_sm::north_east() ); }
    if( on_left  && on_bottom ) { mark( smp + point_rel_sm::south_west() ); }
    if( on_right && on_bottom ) { mark( smp + point_rel_sm::south_east() ); }
}

void map::set_suspension_cache_dirty( const int zlev )
{
    if( inbounds_z( zlev ) ) {
        get_cache( zlev ).suspension_cache_dirty = true;
    }
}

void map::set_floor_cache_dirty( const int zlev )
{
    if( inbounds_z( zlev ) ) {
        get_cache( zlev ).floor_cache_dirty.set();
        get_mapbuffer().mark_submap_caches_dirty( {
            .begin = abs_sub,
            .end = abs_sub + point_rel_sm( my_MAPSIZE, my_MAPSIZE ),
            .zlev = zlev,
            .floor = true,
        } );
    }
    // outside_cache and sheltered_cache at z-1 depend on floor_cache at z.
    set_outside_cache_dirty( zlev - 1 );
    // This also means the absorption and sound wall caches are marked dirty there as well.
    set_absorption_cache_dirty( zlev - 1 );
}

void map::set_floor_cache_dirty( const tripoint_bub_ms &p )
{
    if( !inbounds( p ) ) {
        return;
    }
    level_cache &ch = get_cache( p.z() );
    const auto smp = project_to<coords::sm>( p );
    ch.floor_cache_dirty.set( static_cast<size_t>( ch.bidx( smp.x(), smp.y() ) ) );
    const auto abs_sm = map_local_to_abs( *this, tripoint_bub_sm{ smp.x(), smp.y(), p.z() } );
    get_mapbuffer().mark_submap_caches_dirty( {
        .begin = abs_sm.xy(),
        .end = abs_sm.xy() + point_rel_sm( 1, 1 ),
        .zlev = abs_sm.z(),
        .floor = true,
    } );
    // outside_cache and sheltered_cache at z-1 depend on floor_cache at z.
    // The 3×3 neighbourhood means adjacent submaps at z-1 may also be affected.
    set_outside_cache_dirty( p + tripoint_rel_ms::below() );
    // Setting the floor cache for a submap dirty should also automatically set the absorption cache and sound_wall caches dirty.
    set_absorption_cache_dirty( p + tripoint_rel_ms::below() );
}

void map::set_seen_cache_dirty( const int &zlevel )
{
    if( inbounds_z( zlevel ) ) {
        level_cache &cache = get_cache( zlevel );
        cache.seen_cache_dirty = true;
    }
}

void map::set_transparency_cache_dirty( const tripoint_bub_ms &p )
{
    if( inbounds( p ) ) {
        const auto smp = project_to<coords::sm>( p );
        level_cache &ch = get_cache( smp.z() );
        ch.transparency_cache_dirty.set( static_cast<size_t>( ch.bidx( smp.x(), smp.y() ) ) );
        const auto abs_sm = map_local_to_abs( *this, smp );
        get_mapbuffer().mark_submap_caches_dirty( {
            .begin = abs_sm.xy(),
            .end = abs_sm.xy() + point_rel_sm( 1, 1 ),
            .zlev = abs_sm.z(),
            .transparency = true,
        } );
    }
}

void map::set_absorption_cache_dirty( const tripoint_bub_ms &p )
{
    // Logic lifted shamelessly from set_outside_cache_dirty
    if( !inbounds( p ) ) {
        return;
    }
    level_cache &ch = get_cache( p.z() );
    const auto proj = project_remain<coords::sm>( p );
    const auto smp = proj.quotient_tripoint;
    const auto l = proj.remainder;

    // Helper: mark one submap grid cell dirty in both the bitset and the submap flag.
    auto mark = [&]( const tripoint_bub_sm & p ) {
        if( p.x() < 0 || p.y() < 0 || p.x() >= my_MAPSIZE || p.y() >= my_MAPSIZE ) {
            return;
        }
        ch.absorption_cache_dirty.set( static_cast<size_t>( ch.bidx( p.x(), p.y() ) ) );
        const auto abs_sm = map_local_to_abs( *this, tripoint_bub_sm{ p.x(), p.y(), p.z() } );
        get_mapbuffer().mark_submap_caches_dirty( {
            .begin = abs_sm.xy(),
            .end = abs_sm.xy() + point_rel_sm( 1, 1 ),
            .zlev = abs_sm.z(),
            .absorption = true,
        } );
    };

    // Always mark the tile's own submap.
    mark( smp );

    // rebuild_absorption_cache checks a 3×3 tile neighbourhood, so a tile on a
    // submap boundary can affect tiles in the adjacent submap.
    const bool on_left   = ( l.x() == 0 );
    const bool on_right  = ( l.x() == SEEX - 1 );
    const bool on_top    = ( l.y() == 0 );
    const bool on_bottom = ( l.y() == SEEY - 1 );

    if( on_left )   { mark( smp + point_rel_sm::west() ); }
    if( on_right )  { mark( smp + point_rel_sm::east() ); }
    if( on_top )    { mark( smp + point_rel_sm::north() ); }
    if( on_bottom ) { mark( smp + point_rel_sm::south() ); }

    // Corner neighbours when on both x and y boundaries.
    if( on_left  && on_top )    { mark( smp + point_rel_sm::north_west() ); }
    if( on_right && on_top )    { mark( smp + point_rel_sm::north_east() ); }
    if( on_left  && on_bottom ) { mark( smp + point_rel_sm::south_west() ); }
    if( on_right && on_bottom ) { mark( smp + point_rel_sm::south_east() ); }

}

void map::set_absorption_cache_dirty( const int zlev )
{
    if( inbounds_z( zlev ) ) {
        get_cache( zlev ).absorption_cache_dirty.set();
    }
}

static submap null_submap( tripoint_abs_sm::zero(), dimension_id{} );

maptile map::maptile_at( const tripoint_bub_ms &p ) const
{
    return maptile_at_internal( p );
}

maptile map::maptile_at_internal( const tripoint_bub_ms &p ) const
{
    point_sm_ms l;
    submap *const sm = get_submap_at( tripoint_bub_ms( p ), l );
    if( sm == nullptr ) {
        return maptile( &null_submap, point_sm_ms{} );
    }

    return maptile( sm, l );
}

// Vehicle functions


VehicleList map::get_vehicles()
{
    if( last_full_vehicle_list_dirty ) {
        last_full_vehicle_list = get_vehicles( bubble_submaps().min(),
                                               bubble_submaps().max() );

        last_full_vehicle_list_dirty = false;
    }

    return last_full_vehicle_list;
}

void map::reset_vehicle_cache( )
{
    last_full_vehicle_list_dirty = true;
    clear_vehicle_cache();

    // Cache all vehicles
    for( int zlev = -OVERMAP_DEPTH; zlev <= OVERMAP_HEIGHT; zlev++ ) {
        auto &ch = get_cache( zlev );
        for( const auto &elem : ch.vehicle_list ) {
            elem->adjust_zlevel( 0, tripoint_rel_ms::zero() );
            add_vehicle_to_cache( elem );
        }
    }
}

void map::add_vehicle_to_cache( vehicle *veh )
{
    if( veh == nullptr ) {
        debugmsg( "Tried to add null vehicle to cache" );
        return;
    }

    // Get parts
    for( const vpart_reference &vpr : veh->get_all_parts() ) {
        if( vpr.part().removed ) {
            continue;
        }
        const auto p = abs_to_map_local( *this, veh->abs_part_location( vpr.part() ) );
        int part = veh->part_with_feature( vpr.part_index(), VPFLAG_LADDER, true );
        if( part != -1 ) {
            // NOTE: This cache may need to be submapfied at some point
            // The rope hangs DOWN from the ladder part, so register the whole column from
            // the part down to ladder_length() below it: has_rope_at() and the climb-up /
            // rope-rendering paths look up the tile BELOW the part, not just the top tile
            // (issue #9590). The top tile is kept so climbing down while boarded resolves.
            const auto len = veh->part( part ).info().ladder_length();
            const auto min_z = std::max( p.z() - len, -OVERMAP_DEPTH );
            for( const auto z : std::views::iota( min_z, p.z() + 1 ) ) {
                cached_veh_rope[tripoint_bub_ms( p.xy(), z )] = std::make_pair( veh, static_cast<int>( part ) );
            }
        }
        level_cache &ch = get_cache( p.z() );
        ch.veh_in_active_range = true;

        // DANGER: Unlike what you think where you can just use vpr.has_flag( VPFLAG_NOCOLLIDE )
        // THAT DOES NOT WORK DO NOT TRY AND CHANGE THIS MESS
        if( !ch.veh_cached_parts.contains( p ) ||
            ( !veh->part_info( vpr.part_index() ).has_flag( VPFLAG_NOCOLLIDE ) ) ) {
            ch.veh_cached_parts[p] = std::make_pair( veh,  static_cast<int>( vpr.part_index() ) );
        }
        if( inbounds( p ) ) {
            ch.veh_exists_at[ch.idx( p.x(), p.y() )] = true;
        }
    }

    last_full_vehicle_list_dirty = true;
}

void map::clear_vehicle_point_from_cache( vehicle *veh, const tripoint_bub_ms &pt )
{
    if( veh == nullptr ) {
        debugmsg( "Tried to clear null vehicle from cache" );
        return;
    }

    level_cache &ch = get_cache( pt.z() );
    auto it = ch.veh_cached_parts.find( pt );
    if( it != ch.veh_cached_parts.end() && it->second.first == veh ) {
        if( inbounds( pt ) ) {
            ch.veh_exists_at[ch.idx( pt.x(), pt.y() )] = false;
        }
        ch.veh_cached_parts.erase( it );
        // The rope-ladder cache stores the whole hanging column (see add_vehicle_to_cache),
        // so a bare erase( pt ) would leave the rope tiles below the part. When pt is one of
        // this vehicle's rope tiles (a column top), drop every tile this vehicle owns in that
        // column. The gate keeps the common (no rope) path at a single lookup; the scan does
        // NOT break on gaps, so an interleaved column from another vehicle can't strand this
        // vehicle's lower tiles, and only this vehicle's entries are removed. Index-free on
        // purpose: part indices may be stale here (this can run mid-part_removal_cleanup).
        if( const auto top = cached_veh_rope.find( pt );
            top != cached_veh_rope.end() && top->second.first == veh ) {
            for( const auto z : std::views::iota( -OVERMAP_DEPTH, pt.z() + 1 ) ) {
                const auto col_it = cached_veh_rope.find( tripoint_bub_ms( pt.xy(), z ) );
                if( col_it != cached_veh_rope.end() && col_it->second.first == veh ) {
                    cached_veh_rope.erase( col_it );
                }
            }
        }
    }

}

void map::clear_vehicle_cache( )
{
    for( int zlev = -OVERMAP_DEPTH; zlev <= OVERMAP_HEIGHT; zlev++ ) {
        level_cache &ch = get_cache( zlev );
        while( !ch.veh_cached_parts.empty() ) {
            const auto part = ch.veh_cached_parts.begin();
            const auto &p = part->first;
            if( inbounds( p ) ) {
                ch.veh_exists_at[ch.idx( p.x(), p.y() )] = false;
            }
            ch.veh_cached_parts.erase( part );
        }
        ch.veh_in_active_range = false;
    }
    cached_veh_rope.clear();
}

void map::clear_vehicle_list( const int zlev )
{
    auto &ch = get_cache( zlev );
    ch.vehicle_list.clear();
    ch.zone_vehicles.clear();

    last_full_vehicle_list_dirty = true;
}

void map::update_vehicle_list( const submap *const to, const int zlev )
{
    if( to == nullptr ) {
        return;
    }
    // Update vehicle data
    level_cache &ch = get_cache( zlev );
    for( const auto &elem : to->vehicles ) {
        ch.vehicle_list.insert( elem.get() );
        if( !elem->loot_zones.empty() ) {
            ch.zone_vehicles.insert( elem.get() );
        }
    }

    last_full_vehicle_list_dirty = true;
}

std::unique_ptr<vehicle> map::detach_vehicle( vehicle *veh )
{
    if( veh == nullptr ) {
        debugmsg( "map::detach_vehicle was passed nullptr" );
        return std::unique_ptr<vehicle>();
    }

    int z = veh->abs_sm_pos.z();
    if( z < -OVERMAP_DEPTH || z > OVERMAP_HEIGHT ) {
        debugmsg( "detach_vehicle got a vehicle outside allowed z-level range!  name=%s, submap:%d,%d,%d",
                  veh->name, veh->abs_sm_pos.x(), veh->abs_sm_pos.y(), veh->abs_sm_pos.z() );
        // Try to fix by moving the vehicle here
        z = veh->abs_sm_pos.z() = z > OVERMAP_HEIGHT ? OVERMAP_HEIGHT : -OVERMAP_DEPTH;
    }

    struct detached_vehicle_footprint {
        tripoint_abs_sm min;
        tripoint_abs_sm max;
    };

    auto footprints = std::array<std::optional<detached_vehicle_footprint>, OVERMAP_LAYERS> {};
    for( const vpart_reference &vp : veh->get_all_parts() ) {
        if( vp.part().removed ) {
            continue;
        }
        const auto part_sm = project_to<coords::sm>( veh->abs_part_location( vp.part() ) );
        if( !inbounds_z( part_sm.z() ) ) {
            continue;
        }
        auto &footprint = footprints[part_sm.z() + OVERMAP_DEPTH];
        if( !footprint ) {
            footprint = detached_vehicle_footprint{ .min = part_sm, .max = part_sm };
            continue;
        }
        footprint->min.x() = std::min( footprint->min.x(), part_sm.x() );
        footprint->min.y() = std::min( footprint->min.y(), part_sm.y() );
        footprint->max.x() = std::max( footprint->max.x(), part_sm.x() );
        footprint->max.y() = std::max( footprint->max.y(), part_sm.y() );
    }

    const auto mark_detached_vehicle_footprint_dirty = [&]() {
        for( const auto &footprint : footprints ) {
            if( !footprint ) {
                continue;
            }
            on_vehicle_moved( abs_to_bub( footprint->min ), abs_to_bub( footprint->max ),
                              footprint->min.z() );
        }
    };

    // Unboard all passengers before detaching
    for( auto const &part : veh->get_avail_parts( VPFLAG_BOARDABLE ) ) {
        player *passenger = part.get_passenger();
        if( passenger ) {
            unboard_vehicle( part, passenger );
        }
    }
    veh->invalidate_towing( true );
    submap *current_submap = get_mapbuffer().lookup_submap_in_memory(
                                 veh->abs_sm_pos );
    if( current_submap == nullptr ) {
        debugmsg( "detach_vehicle can't find submap!  name=%s, submap:%d,%d,%d",
                  veh->name, veh->abs_sm_pos.x(), veh->abs_sm_pos.y(), veh->abs_sm_pos.z() );
        get_mapbuffer().unregister_vehicle( veh );
        dirty_vehicle_list.erase( veh );
        mark_detached_vehicle_footprint_dirty();
        return std::unique_ptr<vehicle>();
    }
    level_cache &ch = get_cache( z );
    for( size_t i = 0; i < current_submap->vehicles.size(); i++ ) {
        if( current_submap->vehicles[i].get() == veh ) {
            ch.vehicle_list.erase( veh );
            ch.zone_vehicles.erase( veh );
            reset_vehicle_cache( );
            std::unique_ptr<vehicle> result = std::move( current_submap->vehicles[i] );
            current_submap->vehicles.erase( current_submap->vehicles.begin() + i );
            get_mapbuffer().unregister_vehicle( veh );
            if( veh->tracking_on ) {
                get_overmapbuffer( bound_dimension_ ).remove_vehicle( veh );
            }
            dirty_vehicle_list.erase( veh );
            mark_detached_vehicle_footprint_dirty();
            veh->detach();
            veh->refresh_position();
            return result;
        }
    }
    debugmsg( "detach_vehicle can't find it!  name=%s, submap:%d,%d,%d", veh->name, veh->abs_sm_pos.x(),
              veh->abs_sm_pos.y(), veh->abs_sm_pos.z() );
    return std::unique_ptr<vehicle>();
}

void map::destroy_vehicle( vehicle *veh )
{
    detach_vehicle( veh );
}

void map::on_vehicle_moved( const tripoint_bub_sm &sm_min, const tripoint_bub_sm &sm_max,
                            const int &smz )
{
    ZoneScoped;

    if( !inbounds_z( smz ) ) {
        return;
    }

    auto &ch = get_cache( smz );
    // Vehicle-only caches are cleared by build_map_cache() when a z-level has vehicle
    // cache effects.  Keep that cleanup path active even if this movement is a
    // removal of the last vehicle on the level.
    ch.veh_in_active_range = true;
    invalidate_lightmap_caches();
    set_seen_cache_dirty( smz );
    mark_visibility_cache_dirty( smz );
#if defined( CATA_SDL )
    if( cata_compute::uses_sdl_gpu_compute() ) {
        cata_gpu::invalidate_lighting_transparency_levels( std::vector<int> { smz } );
    }
#endif

    const auto for_clamped_submaps = [&]( const point_bub_sm & range_min,
    const point_bub_sm & range_max, const auto & callback ) {
        const auto bubble_bounds = reality_bubble_2D_bounds();
        const auto requested = inclusive_rectangle<point_bub_sm>( range_min, range_max );
        if( !bubble_bounds.overlaps( requested ) ) {
            return;
        }
        for( const auto p : point_range<point_bub_sm>(
                 clamp( range_min, bubble_bounds ), clamp( range_max, bubble_bounds ) ) ) {
            callback( p );
        }
    };

    // Mark dirty only the submaps the vehicle actually occupies (union of old
    // and new footprint), rather than the entire z-level.
    for_clamped_submaps( sm_min.xy(), sm_max.xy(), [&]( const point_bub_sm & p ) {
        const auto idx = static_cast<size_t>( ch.bidx( p.x(), p.y() ) );
        ch.transparency_cache_dirty.set( idx );
        ch.floor_cache_dirty.set( idx );
        const auto abs_sm = map_local_to_abs( *this, tripoint_bub_sm( p, smz ) );
        get_mapbuffer().mark_submap_caches_dirty( {
            .begin = abs_sm.xy(),
            .end = abs_sm.xy() + point_rel_sm( 1, 1 ),
            .zlev = abs_sm.z(),
            .transparency = true,
            .floor = true,
            .pathfinding = true,
        } );
    } );

    // outside_cache has a 3x3 tile neighbourhood dependency, so expand the
    // dirty region by one submap in each direction.
    const auto outside_min = point_bub_sm( sm_min.x() - 1, sm_min.y() - 1 );
    const auto outside_max = point_bub_sm( sm_max.x() + 1, sm_max.y() + 1 );
    for_clamped_submaps( outside_min, outside_max, [&]( const point_bub_sm & p ) {
        const auto idx = static_cast<size_t>( ch.bidx( p.x(), p.y() ) );
        ch.outside_cache_dirty.set( idx );
        const auto abs_sm = map_local_to_abs( *this, tripoint_bub_sm( p, smz ) );
        get_mapbuffer().mark_submap_caches_dirty( {
            .begin = abs_sm.xy(),
            .end = abs_sm.xy() + point_rel_sm( 1, 1 ),
            .zlev = abs_sm.z(),
            .outside = true,
        } );
    } );

    // Vehicles can extend through the floor; mark the level above as well.
    const auto above_z = smz + 1;
    if( inbounds_z( above_z ) ) {
        auto &ch_above = get_cache( above_z );
        set_seen_cache_dirty( above_z );
        mark_visibility_cache_dirty( above_z );
        for_clamped_submaps( sm_min.xy(), sm_max.xy(), [&]( const point_bub_sm & p ) {
            ch_above.floor_cache_dirty.set(
                static_cast<size_t>( ch_above.bidx( p.x(), p.y() ) ) );
            const auto abs_sm = map_local_to_abs( *this, tripoint_bub_sm( p, above_z ) );
            get_mapbuffer().mark_submap_caches_dirty( {
                .begin = abs_sm.xy(),
                .end = abs_sm.xy() + point_rel_sm( 1, 1 ),
                .zlev = abs_sm.z(),
                .floor = true,
            } );
        } );
    }
}

void map::vehmove()
{
    ZoneScoped;

    // Give vehicles movement points.  Use per-z-level vehicle_list caches
    // rebuilt from in-bubble grid submaps during shift.  Out-of-bubble
    // vehicles are handled by batch_turns_vehicle().
    VehicleList vehicle_list;
    {
        ZoneScopedN( "veh_gain_moves" );
        for( int z = -OVERMAP_DEPTH; z <= OVERMAP_HEIGHT; ++z ) {
            for( vehicle *veh : get_cache( z ).vehicle_list ) {
                veh->gain_moves();
                veh->slow_leak();
                vehicle_list.push_back( wrapped_vehicle{ .pos = abs_to_map_local( *this,
                                        veh->abs_ms_location() ), .v = veh } );
            }
        }
    }
    TracyPlot( "Vehicles Active", static_cast<int64_t>( vehicle_list.size() ) );

    // Priority queue keyed on of_turn (max-heap) for O(log V) scheduling
    // instead of the previous O(V) linear scan per iteration.
    auto veh_cmp = []( const wrapped_vehicle * a, const wrapped_vehicle * b ) {
        return a->v->of_turn < b->v->of_turn;
    };
    using VehPQ = std::priority_queue<wrapped_vehicle *,
          std::vector<wrapped_vehicle *>,
          decltype( veh_cmp )>;
    VehPQ pq( veh_cmp );

    // Separate list of falling / aircraft-z-change vehicles, built alongside the PQ.
    std::vector<wrapped_vehicle *> falling_vehicles;

    // (Re)build pq from vehicle_list, applying the stationary-vehicle filter.
    // Also rebuilds falling_vehicles.
    auto rebuild_pq = [&]() {
        // Use swap-with-empty instead of repeated pop() to clear the queue.
        // pop() calls std::pop_heap which invokes veh_cmp, which dereferences
        // wrapped_vehicle* pointers.  If vehicle_list was just reassigned
        // (vehicle-destroyed path), those pointers are dangling; the comparator
        // dereference would be UB and can corrupt the heap allocator metadata.
        { VehPQ tmp( veh_cmp ); std::swap( pq, tmp ); }
        falling_vehicles.clear();
        for( wrapped_vehicle &w : vehicle_list ) {
            // gain_moves() sets of_turn=0.001 for velocity==0 non-falling
            // non-autopilot vehicles.  Moving and falling vehicles receive
            // of_turn = 1 + carry >= 1.0, so this threshold is unambiguous.
            if( w.v->of_turn >= 1.0f ) {
                pq.push( &w );
            }
            if( w.v->is_falling || ( w.v->is_aircraft() && w.v->get_z_change() != 0 ) ) {
                falling_vehicles.push_back( &w );
            }
        }
    };
    rebuild_pq();

    // the update_active_range lambda that previously scanned all
    // veh_exists_at cells (up to MAPSIZE_X * MAPSIZE_Y per z-level) after every
    // vehicle move has been removed.  veh_in_active_range is maintained
    // incrementally by the existing update_vehicle_list() / vehicle removal
    // paths; a full per-move scan is redundant and O(M²) in the worst case.

    // 15 equals 3 >50mph vehicles, or up to 15 slow (1 square move) ones
    // But 15 is too low for V12 death-bikes, let's put 100 here
    auto moved_count = int64_t{0};
    for( int count = 0; count < 100; ++count ) {
        wrapped_vehicle *cur_veh = nullptr;

        // Horizontal movement — pop highest-of_turn vehicle from heap.
        if( !pq.empty() ) {
            cur_veh = pq.top();
            pq.pop();
        }

        // Vertical-only fallback (falling / aircraft z-change).
        // Scan the pre-built falling_vehicles list (O(falling_count)) instead of
        // the full vehicle_list.  Entries are re-checked for staleness — a vehicle
        // may have landed or been destroyed since falling_vehicles was last built.
        // Vehicles that start falling mid-turn (not in falling_vehicles) are caught
        // on the next turn when rebuild_pq() sees their updated state.
        if( cur_veh == nullptr ) {
            for( wrapped_vehicle *w : falling_vehicles ) {
                if( w->v != nullptr &&
                    ( w->v->is_falling ||
                      ( w->v->is_aircraft() && w->v->get_z_change() != 0 ) ) ) {
                    cur_veh = w;
                    break;
                }
            }
        }

        if( cur_veh == nullptr ) {
            break;
        }

        {
            ZoneScopedN( "veh_act_on_map" );
            ++moved_count;
            cur_veh->v = cur_veh->v->act_on_map();
        }

        if( cur_veh->v == nullptr ) {
            // act_on_map() returns nullptr in two cases:
            //   1. Vehicle destroyed (e.g., fell into void, sank).
            //   2. Vehicle-vehicle collision: move_vehicle() yields to the hit
            //      vehicle by returning nullptr (veh_veh_coll_flag path).
            // In both cases refresh the list so destroyed vehicles are absent
            // and updated of_turn values are visible to rebuild_pq.
            vehicle_list = get_vehicles();
            rebuild_pq();
        } else {
            if( cur_veh->v->of_turn > 0.f ) {
                pq.push( cur_veh );
            }
        }
    }
    TracyPlot( "Vehicles Moved", moved_count );
    static_cast<void>( moved_count );

    // A map shift can occur mid-loop when the player is a vehicle passenger:
    if( last_full_vehicle_list_dirty ) {
        vehicle_list = get_vehicles();
    }

    // Process item removal on the vehicles that were modified this turn.
    // Use a copy because part_removal_cleanup can modify the container.
    {
        ZoneScopedN( "veh_cleanup" );
        auto temp = dirty_vehicle_list;
        for( const auto &elem : temp ) {
            auto same_ptr = [ elem ]( const struct wrapped_vehicle & tgt ) {
                return elem == tgt.v;
            };
            if( std::ranges::find_if( vehicle_list, same_ptr ) !=
                vehicle_list.end() ) {
                elem->part_removal_cleanup();
            }
        }
        dirty_vehicle_list.clear();
    }
    // Build connected_vehicles from the full loaded-vehicle set.
    // All vehicles in vehicle_list are loaded (on_map=true); distribution-graph
    // neighbours reachable but not in the set get on_map=false as before.
    std::set<vehicle *> all_veh_ptrs;
    auto &vehicle_buffer = get_mapbuffer();
    std::ranges::for_each( vehicle_list, [&]( const wrapped_vehicle & w ) {
        if( vehicle_buffer.has_loaded_vehicle( w.v ) ) {
            all_veh_ptrs.insert( w.v );
        }
    } );
    std::map<vehicle *, bool> connected_vehicles;
    vehicle::enumerate_vehicles( connected_vehicles, all_veh_ptrs );
    std::ranges::for_each( connected_vehicles, []( std::pair<vehicle *const, bool> &veh_pair ) {
        veh_pair.first->idle( veh_pair.second );
    } );
}

bool map::vehproceed( VehicleList &vehicle_list )
{
    wrapped_vehicle *cur_veh = nullptr;
    float max_of_turn = 0;
    // First horizontal movement
    for( wrapped_vehicle &vehs_v : vehicle_list ) {
        if( vehs_v.v->of_turn > max_of_turn ) {
            cur_veh = &vehs_v;
            max_of_turn = cur_veh->v->of_turn;
        }
    }

    // Then vertical-only movement
    if( cur_veh == nullptr ) {
        for( wrapped_vehicle &vehs_v : vehicle_list ) {
            if( vehs_v.v->is_falling || ( vehs_v.v->is_aircraft() && vehs_v.v->get_z_change() != 0 ) ) {
                cur_veh = &vehs_v;
                break;
            }
        }
    }

    if( cur_veh == nullptr ) {
        return false;
    }

    cur_veh->v = cur_veh->v->act_on_map();
    if( cur_veh->v == nullptr ) {
        vehicle_list = get_vehicles();
    }

    // confirm that veh_in_active_range is still correct for each z-level
    for( int zlev = -OVERMAP_DEPTH; zlev <= OVERMAP_HEIGHT; ++zlev ) {
        level_cache &cache = get_cache( zlev );

        // Check if any vehicles exist in the active range for this z-level
        cache.veh_in_active_range = cache.veh_in_active_range &&
                                    std::ranges::any_of( cache.veh_exists_at,
        []( bool veh_exists ) {
            return veh_exists;
        } );
    }

    return true;
}

static bool sees_veh( const Creature &c, vehicle &veh, bool force_recalc )
{
    const auto &veh_points = veh.get_points( force_recalc );
    return std::ranges::any_of( veh_points, [&c]( const tripoint_abs_ms & pt ) {
        return c.sees( abs_to_bub( pt ) );
    } );
}

vehicle *map::move_vehicle( vehicle &veh, const tripoint_rel_ms &dp, const tileray &facing )
{
    if( dp == tripoint_rel_ms::zero() ) {
        debugmsg( "Empty displacement vector" );
        return &veh;
    } else if( std::abs( dp.x() ) > 1 || std::abs( dp.y() ) > 1 || std::abs( dp.z() ) > 1 ) {
        debugmsg( "Invalid displacement vector: %d, %d, %d", dp.x(), dp.y(), dp.z() );
        return &veh;
    }
    // Split the movement into horizontal and vertical for easier processing
    if( dp.xy() != point_rel_ms::zero() && dp.z() != 0 ) {
        vehicle *const new_pointer = move_vehicle( veh, tripoint_rel_ms( dp.xy(), 0 ), facing );
        if( !new_pointer ) {
            return nullptr;
        }

        vehicle *const result = move_vehicle( *new_pointer, tripoint_rel_ms( 0, 0, dp.z() ), facing );
        if( !result ) {
            return nullptr;
        }

        result->is_falling = false;
        return result;
    }
    const bool vertical = dp.z() != 0;
    // Ensured by the splitting above
    assert( vertical == ( dp.xy() == point_rel_ms::zero() ) );

    const int target_z = dp.z() + veh.abs_sm_pos.z();
    if( target_z < -OVERMAP_DEPTH || target_z > OVERMAP_HEIGHT ) {
        return &veh;
    }

    veh.precalc_mounts( 1, veh.skidding ? veh.turn_dir : facing.dir(), veh.pivot_point() );

    // cancel out any movement of the vehicle due only to a change in pivot
    // Pivot displacement is a point_rel_veh... Dunno how to convert that
    tripoint_rel_ms dp1 = tripoint_rel_ms( dp - veh.pivot_displacement() );

    if( !vertical ) {
        veh.adjust_zlevel( 1, dp1 );
    }

    int impulse = 0;

    std::vector<veh_collision> collisions;
    std::vector<vehicle *> passthrough;

    // Find collisions
    // Velocity of car before collision
    // Split into vertical and horizontal movement
    const int &coll_velocity = vertical ? veh.vertical_velocity : veh.velocity;
    const int velocity_before = coll_velocity;
    if( velocity_before == 0 && !veh.is_aircraft() && !veh.is_flying_in_air() ) {
        debugmsg( "%s tried to move %s with no velocity",
                  veh.name, vertical ? "vertically" : "horizontally" );
        return &veh;
    }

    bool veh_veh_coll_flag = false;
    // Try to collide multiple times
    size_t collision_attempts = 10;
    do {
        collisions.clear();
        veh.collision( vehicle_collision_options{
            .colls = collisions,
            .dp = dp1,
        } );

        // Vehicle collisions
        std::map<vehicle *, std::vector<veh_collision> > veh_collisions;
        for( auto &coll : collisions ) {
            if( coll.type != veh_coll_veh ) {
                continue;
            }

            veh_veh_coll_flag = true;
            // Only collide with each vehicle once
            veh_collisions[ static_cast<vehicle *>( coll.target ) ].push_back( coll );
        }

        for( auto &pair : veh_collisions ) {
            impulse += vehicle_vehicle_collision( veh, *pair.first, pair.second );
        }

        // Non-vehicle collisions
        for( const auto &coll : collisions ) {
            if( coll.type == veh_coll_veh ) {
                continue;
            }
            if( coll.type == veh_coll_veh_nocollide ) {
                passthrough.push_back( static_cast<vehicle *>( coll.target ) );
                continue;
            }
            if( coll.part > veh.part_count() ||
                veh.part( coll.part ).removed ) {
                continue;
            }

            tripoint_mnt_veh collision_point = veh.part( coll.part ).mount;
            const int coll_dmg = coll.imp;
            // Shock damage, if the target part is a rotor treat as an aimed hit.
            if( veh.part_info( coll.part ).rotor_diameter() > 0 ) {
                veh.damage( coll.part, coll_dmg, DT_BASH, true );
            } else {
                impulse += coll_dmg;
                veh.damage( coll.part, coll_dmg, DT_BASH );
                // Upper bound of shock damage
                int shock_max = coll_dmg;
                // Lower bound of shock damage
                int shock_min = coll_dmg / 2;
                float coll_part_bash_resist = veh.part_info( coll.part ).damage_reduction.type_resist(
                                                  DT_BASH );
                // Reduce shock damage by collision part DR to prevent bushes from damaging car batteries
                shock_min = std::max<int>( 0, shock_min - coll_part_bash_resist );
                shock_max = std::max<int>( 0, shock_max - coll_part_bash_resist );
                // Shock damage decays exponentially, we only want to track shock damage that would cause meaningful damage.
                if( shock_min >= 20 ) {
                    veh.damage_all( shock_min, shock_max, DT_BASH, collision_point );
                }
            }
        }

        // prevent vehicle bouncing after the first collision
        if( vertical && velocity_before < 0 && coll_velocity > 0 ) {
            veh.vertical_velocity = 0; // also affects `coll_velocity` and thus exits the loop
        }

    } while( collision_attempts-- > 0 && coll_velocity != 0 &&
             sgn( coll_velocity ) == sgn( velocity_before ) &&
             !collisions.empty() && !veh_veh_coll_flag );

    const int velocity_after = coll_velocity;
    bool can_move = velocity_after != 0 && sgn( velocity_after ) == sgn( velocity_before );
    if( dp.z() != 0 && veh.is_aircraft() ) {
        can_move = true;
    }
    units::angle coll_turn = 0_degrees;
    if( impulse > 0 ) {
        coll_turn = shake_vehicle( veh, velocity_before, facing.dir() );
        veh.stop_autodriving();
        const int volume = std::min<int>( 120, std::sqrt( impulse ) );
        // TODO: Center the sound at weighted (by impulse) average of collisions
        sound_event se;
        se.origin = veh.bub_ms_location();
        se.volume = volume;
        se.category = sounds::sound_t::combat;
        se.description = _( "crash!" );
        se.id = "smash_success";
        se.variant = "hit_vehicle";
        sounds::sound( se );
    }

    if( veh_veh_coll_flag ) {
        // Break here to let the hit vehicle move away
        return nullptr;
    }

    // If not enough wheels, mess up the ground a bit.
    if( !vertical && !veh.valid_wheel_config() && !veh.is_in_water() && !veh.is_flying_in_air() &&
        !veh.has_sufficient_lift( true ) &&
        dp.z() == 0 ) {
        veh.velocity += veh.velocity < 0 ? 2000 : -2000;
        for( const auto &p : veh.get_points() ) {
            const auto local = abs_to_map_local( *this, p );
            const ter_id &pter = ter( local );
            if( pter == t_dirt || pter == t_grass ) {
                ter_set( local, t_dirtmound );
            }
        }
    }

    const units::angle last_turn_dec = 1_degrees;
    if( veh.last_turn < 0_degrees ) {
        veh.last_turn += last_turn_dec;
        if( veh.last_turn > -last_turn_dec ) {
            veh.last_turn = 0_degrees;
        }
    } else if( veh.last_turn > 0_degrees ) {
        veh.last_turn -= last_turn_dec;
        if( veh.last_turn < last_turn_dec ) {
            veh.last_turn = 0_degrees;
        }
    }

    Character &player_character = get_player_character();
    const bool seen = sees_veh( player_character, veh, false );

    if( can_move || ( vertical && veh.is_falling ) ) {
        // Accept new direction
        if( veh.skidding ) {
            veh.face.init( veh.turn_dir );
        } else {
            veh.face = facing;
        }

        veh.move = facing;
        if( coll_turn != 0_degrees ) {
            veh.skidding = true;
            veh.turn( coll_turn );
        }
        veh.on_move();
        // Actually change position
        displace_vehicle( veh, tripoint_rel_ms( dp1 ) );
        veh.shift_zlevel();
    } else if( !vertical ) {
        veh.stop();
    }
    veh.check_falling_or_floating();
    // If the PC is in the currently moved vehicle, adjust the
    //  view offset.
    if( g->u.controlling_vehicle && veh_pointer_or_null( veh_at( g->u.bub_pos() ) ) == &veh ) {
        g->calc_driving_offset( &veh );
        if( veh.skidding && can_move ) {
            // TODO: Make skid recovery in air hard
            veh.possibly_recover_from_skid();
        }
    }
    // Now we're gonna handle traps we're standing on (if we're still moving).
    if( !vertical && can_move ) {
        const auto wheel_indices = veh.wheelcache; // Don't use a reference here, it causes a crash.

        // Values to deal with crushing items.
        // The math needs to be floating-point to work, so the values might as well be.
        const float vehicle_grounded_wheel_area = static_cast<int>( vehicle_wheel_traction( veh, true ) );
        const float weight_to_damage_factor = 0.05; // Nobody likes a magic number.
        const float vehicle_mass_kg = to_kilogram( veh.total_mass() );

        for( auto &w : wheel_indices ) {
            const auto wheel_p = veh.bub_part_location( w );
            if( one_in( 2 ) && displace_water( wheel_p ) ) {
                sound_event se;
                se.origin = wheel_p;
                se.volume = 50;
                se.category = sounds::sound_t::movement;
                se.movement_noise = true;
                se.description = _( "splash!" );
                se.id = "environment";
                se.variant = "splash";
                sounds::sound( se );
            }

            veh.handle_trap( wheel_p, w );
            if( !has_flag( "SEALED", wheel_p ) ) {
                const float wheel_area =  veh.part( w ).wheel_area();

                // Damage is calculated based on the weight of the vehicle,
                // The area of it's wheels, and the area of the wheel running over the items.
                // This number is multiplied by weight_to_damage_factor to get reasonable results, damage-wise.
                const int wheel_damage = static_cast<int>( ( ( wheel_area / vehicle_grounded_wheel_area ) *
                                         vehicle_mass_kg ) * weight_to_damage_factor );

                //~ %1$s: vehicle name
                smash_items( wheel_p, wheel_damage, string_format( _( "weight of %1$s" ), veh.disp_name() ),
                             false );
            }
        }
    }
    if( veh.is_towing() ) {
        veh.do_towing_move();
        // veh.do_towing_move() may cancel towing, so we need to recheck is_towing here
        if( veh.is_towing() && veh.tow_data.get_towed()->tow_cable_too_far() ) {
            add_msg( m_info, _( "A towing cable snaps off of %s." ),
                     veh.tow_data.get_towed()->disp_name() );
            veh.tow_data.get_towed()->invalidate_towing( true );
        }
    }
    for( vehicle *colveh : passthrough ) {
        g->m.add_vehicle_to_cache( colveh );
    }
    // Redraw scene, but only if the player is not engaged in an activity and
    // the vehicle was seen before or after the move.
    if( !player_character.activity && ( seen || sees_veh( player_character, veh, true ) ) ) {
        g->invalidate_main_ui_adaptor();
        inp_mngr.pump_events();
        ui_manager::redraw_invalidated();
        refresh_display();
    }
    return &veh;
}

float map::vehicle_vehicle_collision( vehicle &veh, vehicle &veh2,
                                      const std::vector<veh_collision> &collisions )
{
    if( &veh == &veh2 ) {
        debugmsg( "Vehicle %s collided with itself", veh.name );
        return 0.0f;
    }

    // Effects of colliding with another vehicle:
    //  transfers of momentum, skidding,
    //  parts are damaged/broken on both sides,
    //  remaining times are normalized
    const veh_collision &c = collisions[0];
    add_msg( m_bad, _( "The %1$s's %2$s collides with %3$s's %4$s." ),
             veh.name,  veh.part_info( c.part ).name(),
             veh2.name, veh2.part_info( c.target_part ).name() );

    const bool vertical = veh.abs_sm_pos.z() != veh2.abs_sm_pos.z();

    // Used to calculate the epicenter of the collision.
    tripoint_rel_veh epicenter1;
    tripoint_rel_veh epicenter2;

    float veh1_impulse = 0;
    float veh2_impulse = 0;
    float delta_vel = 0;
    // A constant to tune how many Ns of impulse are equivalent to 1 point of damage, look in vehicle_move.cpp for the impulse to damage function.
    const float dmg_adjust = impulse_to_damage( 1 );
    float dmg_veh1 = 0;
    float dmg_veh2 = 0;
    // Vertical collisions will be simpler for a while (1D)
    if( !vertical ) {
        // For reference, a cargo truck weighs ~25300, a bicycle 690,
        //  and 38mph is 3800 'velocity'
        // Converting away from 100*mph, because mixing unit systems is bad.
        // 1 mph = 0.44704m/s = 100 "velocity". For velocity to m/s, *0.0044704
        rl_vec2d velo_veh1 = veh.velo_vec();
        rl_vec2d velo_veh2 = veh2.velo_vec();
        const float m1 = to_kilogram( veh.total_mass() );
        const float m2 = to_kilogram( veh2.total_mass() );

        // Collision_axis
        tripoint_mnt_veh cof1 = veh.rotated_center_of_mass();
        tripoint_mnt_veh cof2 = veh2.rotated_center_of_mass();
        int &x_cof1 = cof1.x();
        int &y_cof1 = cof1.y();
        int &x_cof2 = cof2.x();
        int &y_cof2 = cof2.y();
        rl_vec2d collision_axis_y;

        collision_axis_y.x = ( veh.bub_ms_location().x() + x_cof1 ) - ( veh2.bub_ms_location().x() +
                             x_cof2 );
        collision_axis_y.y = ( veh.bub_ms_location().y() + y_cof1 ) - ( veh2.bub_ms_location().y() +
                             y_cof2 );
        collision_axis_y = collision_axis_y.normalized();
        rl_vec2d collision_axis_x = collision_axis_y.rotated( M_PI / 2 );
        // imp? & delta? & final? reworked:
        // newvel1 =( vel1 * ( mass1 - mass2 ) + ( 2 * mass2 * vel2 ) ) / ( mass1 + mass2 )
        // as per http://en.wikipedia.org/wiki/Elastic_collision
        float vel1_y = cmps_to_mps( collision_axis_y.dot_product( velo_veh1 ) );
        float vel1_x = cmps_to_mps( collision_axis_x.dot_product( velo_veh1 ) );
        float vel2_y = cmps_to_mps( collision_axis_y.dot_product( velo_veh2 ) );
        float vel2_x = cmps_to_mps( collision_axis_x.dot_product( velo_veh2 ) );
        delta_vel = std::abs( vel1_y - vel2_y );
        // Keep in mind get_collision_factor is looking for m/s, not m/h.
        // e = 0 -> inelastic collision
        // e = 1 -> elastic collision
        float e = get_collision_factor( vel1_y - vel2_y );
        add_msg( m_debug, "Requested collision factor, received %.2f", e );

        // Velocity after collision
        // vel1_x_a = vel1_x, because in x-direction we have no transmission of force
        float vel1_x_a = vel1_x;
        float vel2_x_a = vel2_x;
        // Transmission of force only in direction of collision_axix_y
        // Equation: partially elastic collision
        float vel1_y_a = ( ( m2 * vel2_y * ( 1 + e ) + vel1_y * ( m1 - m2 * e ) ) / ( m1 + m2 ) );
        float vel2_y_a = ( ( m1 * vel1_y * ( 1 + e ) + vel2_y * ( m2 - m1 * e ) ) / ( m1 + m2 ) );
        // Add both components; Note: collision_axis is normalized
        rl_vec2d final1 = ( collision_axis_y * vel1_y_a + collision_axis_x * vel1_x_a ) * 100.0;
        rl_vec2d final2 = ( collision_axis_y * vel2_y_a + collision_axis_x * vel2_x_a ) * 100.0;

        veh.move.init( point_rel_ms( final1.as_point() ) );
        if( final1.dot_product( veh.face_vec() ) < 0 ) {
            // Car is being pushed backwards. Make it move backwards
            veh.velocity = -final1.magnitude();
        } else {
            veh.velocity = final1.magnitude();
        }

        veh2.move.init( point_rel_ms( final2.as_point() ) );
        if( final2.dot_product( veh2.face_vec() ) < 0 ) {
            // Car is being pushed backwards. Make it move backwards
            veh2.velocity = -final2.magnitude();
        } else {
            veh2.velocity = final2.magnitude();
        }

        //give veh2 the initiative to proceed next before veh1
        float avg_of_turn = ( veh2.of_turn + veh.of_turn ) / 2;
        if( avg_of_turn < .1f ) {
            avg_of_turn = .1f;
        }

        veh.of_turn = avg_of_turn * .9;
        // Clamp veh2's of_turn to at least 1.0 so the priority-queue scheduler
        // in vehmove() enqueues the hit vehicle for movement this turn.
        // Without this the V-2 pq threshold (>= 1.0) would never be reached by
        // avg * 1.1 alone, leaving the hit vehicle stuck and causing veh1 to
        // repeatedly ram it every subsequent turn (BUG-1 follow-up).
        veh2.of_turn = std::max( 1.0f, avg_of_turn * 1.1f );

        // Remember that the impulse on vehicle 1 is techncally negative, slowing it
        veh1_impulse = std::abs( m1 * ( vel1_y_a - vel1_y ) );
        veh2_impulse = std::abs( m2 * ( vel2_y_a - vel2_y ) );
    } else {
        const float m1 = to_kilogram( veh.total_mass() );
        // Collision is perfectly inelastic for simplicity
        // Assume veh2 is standing still
        dmg_veh1 = ( std::abs( cmps_to_mps( veh.vertical_velocity ) ) * ( m1 / 10 ) ) / 2;
        dmg_veh2 = dmg_veh1;
        veh.vertical_velocity = 0;
    }

    // To facilitate pushing vehicles, because the simulation pretends cars are ping pong balls that get all their velocity in zero starting distance to slam into eachother while touching.
    // Stay under 6 m/s to push cars without damaging them
    if( delta_vel >= 6.0f ) {
        dmg_veh1 = veh1_impulse * dmg_adjust;
        dmg_veh2 = veh2_impulse * dmg_adjust;
    } else {
        dmg_veh1 = 0;
        dmg_veh2 = 0;
    }


    int coll_parts_cnt = 0; //quantity of colliding parts between veh1 and veh2
    for( const auto &veh_veh_coll : collisions ) {
        if( &veh2 == static_cast<vehicle *>( veh_veh_coll.target ) ) {
            coll_parts_cnt++;
        }
    }

    const float dmg1_part = dmg_veh1 / coll_parts_cnt;
    const float dmg2_part = dmg_veh2 / coll_parts_cnt;

    //damage colliding parts (only veh1 and veh2 parts)
    for( const auto &veh_veh_coll : collisions ) {
        if( &veh2 != static_cast<vehicle *>( veh_veh_coll.target ) ) {
            continue;
        }

        int parm1 = veh.part_with_feature( veh_veh_coll.part, VPFLAG_ARMOR, true );
        if( parm1 < 0 ) {
            parm1 = veh_veh_coll.part;
        }
        int parm2 = veh2.part_with_feature( veh_veh_coll.target_part, VPFLAG_ARMOR, true );
        if( parm2 < 0 ) {
            parm2 = veh_veh_coll.target_part;
        }

        // NOTE: This should just be add
        // But for some reason you cant add mnt_veh to rel_veh?
        epicenter1 += veh.part( parm1 ).mount.raw();
        veh.damage( parm1, dmg1_part, DT_BASH );

        epicenter2 += veh2.part( parm2 ).mount.raw();
        veh2.damage( parm2, dmg2_part, DT_BASH );
    }

    epicenter2.x() /= coll_parts_cnt;
    epicenter2.y() /= coll_parts_cnt;

    if( dmg2_part > 100 ) {
        // Shake vehicle because of collision
        // FIXME: I dunno how else to do this, it comes out to a mount but is relative in the meantime
        veh2.damage_all( dmg2_part / 2, dmg2_part, DT_BASH, tripoint_mnt_veh( epicenter2.raw() ) );
    }

    if( dmg_veh1 > 800 ) {
        veh.skidding = true;
    }

    if( dmg_veh2 > 800 ) {
        veh2.skidding = true;
    }

    // Return the impulse of the collision
    return dmg_veh1;
}

bool map::check_vehicle_zones( const int zlev )
{
    for( auto veh : get_cache( zlev ).zone_vehicles ) {
        if( veh->zones_dirty ) {
            return true;
        }
    }
    return false;
}

std::vector<zone_data *> map::get_vehicle_zones( const int zlev )
{
    std::vector<zone_data *> veh_zones;
    bool rebuild = false;
    for( auto veh : get_cache( zlev ).zone_vehicles ) {
        if( veh->refresh_zones() ) {
            rebuild = true;
        }
        for( auto &zone : veh->loot_zones ) {
            veh_zones.emplace_back( &zone.second );
        }
    }
    if( rebuild ) {
        zone_manager::get_manager().cache_vzones();
    }
    return veh_zones;
}

void map::register_vehicle_zone( vehicle *veh, const int zlev )
{
    auto &ch = get_cache( zlev );
    ch.zone_vehicles.insert( veh );
}

bool map::deregister_vehicle_zone( zone_data &zone )
{
    if( const std::optional<vpart_reference> vp = veh_at( abs_to_map_local( *this,
            tripoint_abs_ms( zone.get_start_point() ) ) ).part_with_feature( "CARGO", false ) ) {
        const auto bounds = vp->vehicle().loot_zones.equal_range( vp->mount() );
        const auto it = std::ranges::find_if( std::ranges::subrange( bounds.first, bounds.second ),
        [&zone]( const auto & entry ) {
            return &zone == &entry.second;
        } );
        if( it != bounds.second ) {
            vp->vehicle().loot_zones.erase( it );
            if( vp->vehicle().loot_zones.empty() ) {
                get_cache( vp->vehicle().abs_sm_pos.z() ).zone_vehicles.erase( &vp->vehicle() );
            }
            return true;
        }
    }
    return false;
}

// 3D vehicle functions

VehicleList map::get_vehicles( const tripoint_bub_sm &start, const tripoint_bub_sm &end )
{
    auto chunk_start = start;
    clip_to_bounds( chunk_start );
    auto chunk_end = end;
    clip_to_bounds( chunk_end );
    auto vehs = VehicleList{};

    if( chunk_start.x() > chunk_end.x() || chunk_start.y() > chunk_end.y() ||
        chunk_start.z() > chunk_end.z() ) {
        return vehs;
    }

    for( const auto sm_pos : tripoint_range<tripoint_bub_sm>( chunk_start, chunk_end ) ) {
        const auto view = active_submaps_.get_submap_view( point_rel_sm( sm_pos.x(), sm_pos.y() ),
                          sm_pos.z() );
        if( !view ) {
            continue;
        }
        const submap *current_submap = &view->get_submap();
        for( const auto &elem : current_submap->vehicles ) {
            auto w = wrapped_vehicle{};
            w.v = elem.get();
            w.pos = abs_to_map_local( *this, w.v->abs_ms_location() );
            vehs.push_back( w );
        }
    }

    return vehs;
}

optional_vpart_position map::veh_at( const tripoint_abs_ms &p ) const
{
    return get_mapbuffer().veh_at( p );
}

optional_vpart_position map::veh_at( const tripoint_bub_ms &p ) const
{
    if( !inbounds( p ) || !const_cast<map *>( this )->get_cache( p.z() ).veh_in_active_range ) {
        return optional_vpart_position( std::nullopt );
    }

    auto part_num = 1;
    auto *const veh = const_cast<map *>( this )->veh_at_internal( p, part_num );
    if( !veh ) {
        return optional_vpart_position( std::nullopt );
    }
    return optional_vpart_position( vpart_position( *veh, part_num ) );
}

const vehicle *map::veh_at_internal( const tripoint_bub_ms &p, int &part_num ) const
{
    // This function is called A LOT. Move as much out of here as possible.
    const level_cache &ch = get_cache( p.z() );
    if( !ch.veh_in_active_range || !ch.veh_exists_at[ch.idx( p.x(), p.y() )] ) {
        part_num = -1;
        return nullptr; // Clear cache indicates no vehicle. This should optimize a great deal.
    }

    const auto it = ch.veh_cached_parts.find( p );
    if( it != ch.veh_cached_parts.end() ) {
        part_num = it->second.second;
        return it->second.first;
    }

    debugmsg( "vehicle part cache indicated vehicle not found: %d %d %d", p.x(), p.y(), p.z() );
    part_num = -1;
    return nullptr;
}

vehicle *map::veh_at_internal( const tripoint_bub_ms &p, int &part_num )
{
    return const_cast<vehicle *>( const_cast<const map *>( this )->veh_at_internal( p, part_num ) );
}

void map::board_vehicle( const tripoint_bub_ms &pos, Character *who )
{
    if( who == nullptr ) {
        debugmsg( "map::board_vehicle: null player" );
        return;
    }

    auto vp = veh_at( pos ).part_with_feature( VPFLAG_BOARDABLE, true );
    if( !vp ) {
        const auto abs_pos = map_local_to_abs( *this, pos );
        vp = get_mapbuffer().veh_at( abs_pos, {
            .mode = mapbuffer_lookup_mode::resident_only,
        } ).part_with_feature( VPFLAG_BOARDABLE, true );
    }
    if( !vp ) {
        if( who->grab_point.x() == 0 && who->grab_point.y() == 0 ) {
            debugmsg( "map::board_vehicle: vehicle not found" );
        }
        return;
    }
    if( vp->part().has_flag( vehicle_part::passenger_flag ) ) {
        player *psg = vp->vehicle().get_passenger( vp->part_index() );
        debugmsg( "map::board_vehicle: passenger (%s) is already there",
                  psg ? psg->name : "<null>" );
        unboard_vehicle( pos );
    }
    vp->part().set_flag( vehicle_part::passenger_flag );
    vp->part().passenger_id = who->getID();
    vp->vehicle().invalidate_mass();

    who->setpos( pos );
    who->in_vehicle = true;
}

void map::unboard_vehicle( const vpart_reference &vp, Character *passenger, bool dead_passenger )
{
    // Mark the part as un-occupied regardless of whether there's a live passenger here.
    vp.part().remove_flag( vehicle_part::passenger_flag );
    vp.vehicle().invalidate_mass();

    if( !passenger ) {
        if( !dead_passenger ) {
            debugmsg( "map::unboard_vehicle: passenger not found" );
        }
        return;
    }
    passenger->in_vehicle = false;
    // Only make vehicle go out of control if the driver is the one unboarding.
    if( passenger->controlling_vehicle ) {
        vp.vehicle().skidding = true;
    }
    passenger->controlling_vehicle = false;
}

void map::unboard_vehicle( const tripoint_bub_ms &p, bool dead_passenger )
{
    const std::optional<vpart_reference> vp = veh_at( p ).part_with_feature( VPFLAG_BOARDABLE, false );
    player *passenger = nullptr;
    if( !vp ) {
        debugmsg( "map::unboard_vehicle: vehicle not found" );
        // Try and force unboard the player anyway.
        passenger = g->critter_at<player>( p );
        if( passenger ) {
            passenger->in_vehicle = false;
            passenger->controlling_vehicle = false;
        }
        return;
    }
    passenger = vp->get_passenger();
    unboard_vehicle( *vp, passenger, dead_passenger );
}

bool map::displace_vehicle( vehicle &veh, const tripoint_rel_ms &dp )
{
    const auto src = veh.abs_ms_location();
    const auto dest = src + dp;
    const auto dest_proj = project_remain<coords::sm>( dest );
    const bool sm_shift = veh.abs_sm_pos != dest_proj.quotient_tripoint;

    submap *const src_submap = sm_shift ? MAPBUFFER_REGISTRY.get(
                                   bound_dimension_ ).lookup_submap_in_memory( veh.abs_sm_pos ) : nullptr;
    submap *const dst_submap = sm_shift ? MAPBUFFER_REGISTRY.get(
                                   bound_dimension_ ).lookup_submap_in_memory( dest_proj.quotient_tripoint ) : nullptr;

    std::set<int> smzs;
    size_t our_i = 0;

    if( sm_shift ) {
        if( src_submap == nullptr ) {
            debugmsg( "displace_vehicle: src submap null for '%s' at %d,%d,%d",
                      veh.name, src.x(), src.y(), src.z() );
            return false;
        }

        // Find the vehicle's index directly in its authoritative source submap.
        // get_submap_at() handles out-of-bubble positions via the mapbuffer fallback,
        // so this works for vehicles loaded outside the reality bubble.
        bool found = false;
        for( size_t i = 0; i < src_submap->vehicles.size(); ++i ) {
            if( src_submap->vehicles[i].get() == &veh ) {
                our_i = i;
                found = true;
                break;
            }
        }

        if( !found ) {
            add_msg( m_debug, "displace_vehicle [%s] failed", veh.name );
            return false;
        }

        // Stop the vehicle if its destination submap is not loaded.
        // Safety net for cases where act_on_map consumed movement before collision fired.
        if( dst_submap == nullptr ) {
            veh.stop();
            dbg( DL::Error ) << "map::displace_vehicle: dst submap not loaded, stopping vehicle dp=" << dp;
            return true;
        }
    }

    // Need old coordinates to check for remote control
    const bool remote = veh.remote_controlled( g->u );

    // record every passenger and pet inside
    std::vector<rider_data> riders = veh.get_riders();

    bool need_update = false;
    bool z_change = false;
    // Move passengers and pets
    bool complete = false;
    // loop until everyone has moved or for each passenger
    for( size_t i = 0; !complete && i < riders.size(); i++ ) {
        complete = true;
        for( rider_data &r : riders ) {
            if( r.moved ) {
                continue;
            }
            const int prt = r.prt;

            Creature *psg = r.psg;
            const tripoint_bub_ms part_pos = veh.bub_part_location( prt );
            if( psg == nullptr ) {
                debugmsg( "Empty passenger for part #%d at %d,%d,%d player at %d,%d,%d?",
                          prt, part_pos.x(), part_pos.y(), part_pos.z(),
                          g->u.bub_pos().x(), g->u.bub_pos().y(), g->u.bub_pos().z() );
                veh.part( prt ).remove_flag( vehicle_part::passenger_flag );
                r.moved = true;
                continue;
            }

            if( psg->bub_pos() != part_pos ) {
                add_msg( m_debug, "Part/passenger position mismatch: part #%d at %d,%d,%d "
                         "passenger at %d,%d,%d", prt, part_pos.x(), part_pos.y(), part_pos.z(),
                         psg->bub_pos().x(), psg->bub_pos().y(), psg->bub_pos().z() );
            }
            const vehicle_part &veh_part = veh.part( prt );

            // Place passenger on the new part location.  Z must include mount
            // and terrain-topology offsets — precalc[1] is XY-only.
            auto psgp = abs_to_map_local( *this, dest + tripoint_rel_ms( veh_part.precalc[1].x(),
                                          veh_part.precalc[1].y(),
                                          veh_part.mount.z() + veh_part.z_terrain[1] ) );
            // someone is in the way so try again
            if( g->critter_at( psgp ) ) {
                complete = false;
                continue;
            }
            if( psg->is_avatar() ) {
                need_update = true;
                z_change = psgp.z() != part_pos.z();
            }

            psg->setpos( psgp );
            r.moved = true;
        }
    }

    // Capture the old footprint in absolute submap coordinates BEFORE parts
    // are updated by advance_precalc_mounts.  The player may shift the map
    // origin below, so bubble coordinates would be stale by on_vehicle_moved().
    auto veh_sm_min = tripoint_abs_sm( INT_MAX, INT_MAX, INT_MAX );
    auto veh_sm_max = tripoint_abs_sm( INT_MIN, INT_MIN, INT_MIN );

    auto expand_bounds = [&]( const tripoint_abs_ms & base, const vehicle_part & prt ) {
        const auto p = project_to<coords::sm>( base + tripoint_rel_ms(
                prt.precalc[0], prt.mount.z() + prt.z_terrain[0] ) );
        veh_sm_min.x() = std::min( veh_sm_min.x(), p.x() );
        veh_sm_min.y() = std::min( veh_sm_min.y(), p.y() );
        veh_sm_min.z() = std::min( veh_sm_min.z(), p.z() );
        veh_sm_max.x() = std::max( veh_sm_max.x(), p.x() );
        veh_sm_max.y() = std::max( veh_sm_max.y(), p.y() );
        veh_sm_max.z() = std::max( veh_sm_max.z(), p.z() );
    };

    for( const vpart_reference &vpr : veh.get_all_parts() ) {
        if( !vpr.part().removed ) {
            expand_bounds( src, vpr.part() );
        }
    }

    veh.shed_loose_parts();

    // Clear overlay map memory (vpart only) for every tile the vehicle is vacating.
    // precalc[0] and z_terrain[0] still hold the OLD offsets here (before
    // advance_precalc_mounts), so this gives the authoritative absolute tile
    // positions being left.
    // Terrain memory is preserved so the ground beneath doesn't go black.
    {
        avatar &you = get_avatar();
        const auto clear_matching_overlay = [&]( const tripoint_abs_ms & pos,
        const std::string & tile_id ) {
            if( you.get_memorized_tile( pos ).tile == tile_id ) {
                you.clear_memorized_overlay( pos );
            }
        };

        for( const auto &vpr : veh.get_all_parts() ) {
            if( !vpr.part().removed ) {
                const auto &part = vpr.part();
                const auto part_offset = tripoint_rel_ms(
                                             part.precalc[0],
                                             part.mount.z() + part.z_terrain[0] );
                you.clear_memorized_overlay( src + part_offset );

                const auto &part_info = part.info();
                if( part_info.has_flag( VPFLAG_LADDER ) ) {
                    const auto ladder_pos = src + part_offset;
                    const auto rope_tile = "vp_" + part_info.get_id().str();
                    const auto min_rope_z = std::max( ladder_pos.z() - part_info.ladder_length(),
                                                      -OVERMAP_DEPTH );
                    for( const auto z : std::views::iota( min_rope_z, ladder_pos.z() ) ) {
                        auto rope_pos = ladder_pos;
                        rope_pos.z() = z;
                        clear_matching_overlay( rope_pos, rope_tile );
                    }
                }
            }
        }
    }

    smzs = veh.advance_precalc_mounts( src );
    veh.sm_ms_pos = dest_proj.remainder;

    // Expand bounds with the new footprint (precalc[0] now holds new offsets).
    for( const vpart_reference &vpr : veh.get_all_parts() ) {
        if( !vpr.part().removed ) {
            expand_bounds( dest, vpr.part() );
        }
    }

    if( sm_shift && src_submap != dst_submap ) {
        auto src_submap_veh_it = src_submap->vehicles.begin() + our_i;
        dst_submap->vehicles.push_back( std::move( *src_submap_veh_it ) );
        src_submap->vehicles.erase( src_submap_veh_it );
        dst_submap->is_uniform = false;
        invalidate_max_populated_zlev( dest.z() );

        // Update abs_sm_pos for the submap boundary crossing.
        const auto prev = veh.abs_sm_pos;
        veh.abs_sm_pos = dest_proj.quotient_tripoint;
        veh.update_overmap( prev );
    }
    get_mapbuffer().refresh_vehicle_footprint( &veh );

    //
    //global positions of vehicle loot zones have changed.
    veh.zones_dirty = true;

    auto vehicle_moved_marked = false;
    auto const mark_vehicle_moved = [&]() {
        if( vehicle_moved_marked ) {
            return;
        }
        std::ranges::for_each( smzs, [&]( const int vsmz ) {
            const auto smz = dest.z() + vsmz;
            const auto veh_bub_sm_min = abs_to_bub( tripoint_abs_sm( veh_sm_min.xy(), smz ) );
            const auto veh_bub_sm_max = abs_to_bub( tripoint_abs_sm( veh_sm_max.xy(), smz ) );
            on_vehicle_moved( veh_bub_sm_min, veh_bub_sm_max, smz );
        } );
        vehicle_moved_marked = true;
    };

    if( need_update && !z_change && src.z() == dest.z() ) {
        mark_vehicle_moved();
    }
    add_vehicle_to_cache( &veh );

    if( z_change || src.z() != dest.z() ) {
        if( z_change ) {
            // vertical moves can flush the caches, so make sure we're still in the cache
            add_vehicle_to_cache( &veh );
        }
        update_vehicle_list( dst_submap, dest.z() );
        // delete the vehicle from the source z-level vehicle cache set if it is no longer on
        // that z-level
        if( src.z() != dest.z() ) {
            level_cache &ch2 = get_cache( src.z() );
            for( const vehicle *elem : ch2.vehicle_list ) {
                if( elem == &veh ) {
                    ch2.vehicle_list.erase( &veh );
                    ch2.zone_vehicles.erase( &veh );
                    break;
                }
            }
        }
        veh.check_is_heli_landed();
    }
    if( veh.is_flying_in_air() ) {
        veh.check_is_heli_landed();
    }
    if( remote ) {
        // Has to be after update_map or coordinates won't be valid
        g->setremoteveh( &veh );
    }
    mark_vehicle_moved();
    return true;
}

bool map::displace_water( const tripoint_bub_ms &p )
{
    // Check for shallow water
    if( has_flag( "SWIMMABLE", p ) && !has_flag( TFLAG_DEEP_WATER, p ) ) {
        int dis_places = 0;
        int sel_place = 0;
        for( int pass = 0; pass < 2; pass++ ) {
            // we do 2 passes.
            // first, count how many non-water places around
            // then choose one within count and fill it with water on second pass
            if( pass != 0 ) {
                sel_place = rng( 0, dis_places - 1 );
                dis_places = 0;
            }
            for( const auto &temp : points_in_radius( p, 1 ) ) {
                if( temp != p
                    || impassable_ter_furn( temp )
                    || has_flag( TFLAG_DEEP_WATER, temp ) ) {
                    continue;
                }
                ter_id ter0 = ter( temp );
                if( ter0 == t_water_sh ||
                    ter0 == t_water_dp || ter0 == t_water_moving_sh || ter0 == t_water_moving_dp ) {
                    continue;
                }
                if( pass != 0 && dis_places == sel_place ) {
                    ter_set( temp, t_water_sh );
                    ter_set( temp, t_dirt );
                    return true;
                }

                dis_places++;
            }
        }
    }
    return false;
}

// End of 3D vehicle

void map::set( const tripoint_bub_ms &p, const ter_id &new_terrain, const furn_id &new_furniture )
{
    furn_set( p, new_furniture );
    ter_set( p, new_terrain );
}

std::string map::name( const tripoint_bub_ms &p )
{
    return has_furn( p ) ? furnname( p ) : tername( p );
}

std::string map::disp_name( const tripoint_bub_ms &p )
{
    return string_format( _( "the %s" ), name( p ) );
}

std::string map::obstacle_name( const tripoint_bub_ms &p )
{
    if( const std::optional<vpart_reference> vp = veh_at( p ).obstacle_at_part() ) {
        return vp->info().name();
    }
    return name( p );
}

bool map::has_furn( const tripoint_bub_ms &p ) const
{
    return furn( p ) != f_null;
}

furn_id map::furn( const tripoint_bub_ms &p ) const
{
    if( get_mapbuffer().is_outside_pocket_dimension_bounds( map_local_to_abs( *this, p ) ) ) {
        return f_null;
    }

    point_sm_ms l;
    submap *const current_submap = get_submap_at( tripoint_bub_ms( p ), l );
    if( current_submap == nullptr ) {
        return f_null;
    }

    return current_submap->get_furn( l );
}

void map::furn_set( const tripoint_bub_ms &p, const furn_id &new_furniture,
                    const cata::poly_serialized<active_tile_data> &new_active )
{
    const auto abs_pos = map_local_to_abs( *this, p );
    if( get_mapbuffer().is_outside_pocket_dimension_bounds( abs_pos ) ) {
        return;
    }

    get_mapbuffer().set_furn( abs_pos, {
        .furniture = new_furniture,
        .active = &new_active,
        .lookup = resident_item_lookup(),
    } );
}

auto map::move_furn( const tripoint_bub_ms &from, const tripoint_bub_ms &to,
                     const map_move_furn_options &options ) -> bool
{
    if( from == to ) {
        return true;
    }

    const auto moving_furn = furn( from );
    if( moving_furn == f_null || has_furn( to ) ) {
        return false;
    }

    const auto src_items = i_at( from ).size();
    auto dst_stack = i_at( to );
    const auto dst_items = dst_stack.size();
    const auto only_liquid_items = std::all_of( dst_stack.begin(), dst_stack.end(),
    []( const auto & liquid_item ) {
        return liquid_item->made_of( LIQUID );
    } );

    const auto dst_item_ok = !has_flag( "NOITEM", to ) &&
                             !has_flag( "SWIMMABLE", to ) &&
                             !has_flag( "DESTROY_ITEM", to );

    const auto src_item_ok = moving_furn.obj().has_flag( "CONTAINER" ) ||
                             moving_furn.obj().has_flag( "FIRE_CONTAINER" ) ||
                             moving_furn.obj().has_flag( "SEALED" );

    const auto fire_intensity = options.move_fire ? get_field_intensity( from, fd_fire ) : 0;
    auto fire_age = options.move_fire ? get_field_age( from, fd_fire ) : 0_turns;

    auto *atd = active_tiles::furn_at<active_tile_data>( map_local_to_abs( *this, from ),
                get_mapbuffer() );

    const auto dst_vars = furn_vars( to );
    const auto src_vars = furn_vars( from );
    std::swap( *src_vars, *dst_vars );

    furn_set( to, moving_furn, atd ? atd->clone() : nullptr );
    furn_set( from, f_null );

    if( options.mover != nullptr ) {
        if( auto *const avatar_mover = options.mover->as_avatar() ) {
            avatar_mover->clear_memorized_overlay( map_local_to_abs( *this, from ) );
        }
    }

    if( fire_intensity == 1 && !options.pulling ) {
        remove_field( from, fd_fire );
        set_field_intensity( to, fd_fire, fire_intensity );
        set_field_age( to, fd_fire, fire_age );
    }

    if( dst_items > 0 && only_liquid_items ) {
        i_clear( to );
    }

    if( options.move_contents && src_items > 0 ) {
        if( dst_item_ok && src_item_ok ) {
            auto src_contents = i_clear( from );
            auto dst_contents = i_clear( to );
            for( auto &it : src_contents ) {
                i_at( to ).insert( std::move( it ) );
            }
            for( auto &it : dst_contents ) {
                i_at( from ).insert( std::move( it ) );
            }
        } else {
            add_msg( _( "Stuff spills from the %s!" ), moving_furn.obj().name() );
        }
    }

    return true;
}

static auto release_avatar_grabbed_furniture_if_destroyed( const tripoint_bub_ms &p,
        const furn_t &old_furniture, const furn_id &new_furniture ) -> void
{
    if( g == nullptr ) {
        return;
    }

    avatar &you = get_avatar();
    if( you.get_grab_type() == OBJECT_FURNITURE &&
        you.bub_pos() + you.grab_point == p &&
        !new_furniture.obj().is_movable() ) {
        add_msg( _( "The %s you were grabbing is destroyed!" ), old_furniture.name() );
        you.grab( OBJECT_NONE );
    }
}

bool map::can_move_furniture( const tripoint_bub_ms &pos, player *p )
{
    if( !p ) {
        return false;
    }
    const furn_t &furniture_type = furn( pos ).obj();
    int required_str = furniture_type.move_str_req;

    // Object can not be moved (or nothing there)
    if( required_str < 0 ) {
        return false;
    }

    ///\EFFECT_STR determines what furniture the player can move
    int adjusted_str = p->str_cur;
    if( p->is_mounted() ) {
        auto mons = p->mounted_creature.get();
        if( mons->has_flag( MF_RIDEABLE_MECH ) && mons->mech_str_addition() != 0 ) {
            adjusted_str = mons->mech_str_addition();
        }
    }
    return adjusted_str >= required_str;
}

std::string map::furnname( const tripoint_bub_ms &p )
{
    const furn_t &f = furn( p ).obj();
    if( f.has_flag( "PLANT" ) ) {
        // Can't use item_stack::only_item() since there might be fertilizer
        map_stack items = i_at( p );
        const map_stack::iterator seed = std::ranges::find_if( items,
        []( const item * const & it ) {
            return it->is_seed();
        } );
        if( seed == items.end() ) {
            debugmsg( "Missing seed for plant at (%d, %d, %d)", p.x(), p.y(), p.z() );
            return "null";
        }
        const std::string &plant = ( *seed )->get_plant_name();
        return string_format( "%s (%s)", f.name(), plant );
    } else {
        return f.name();
    }
}

/*
 * Get the terrain integer id. This is -not- a number guaranteed to remain
 * the same across revisions; it is a load order, and can change when mods
 * are loaded or removed. The old t_floor style constants will still work but
 * are -not- guaranteed; if a mod removes t_lava, t_lava will equal t_null;
 * New terrains added to the core game generally do not need this, it's
 * retained for high performance comparisons, save/load, and gradual transition
 * to string terrain.id
 */
ter_id map::ter( const tripoint_bub_ms &p ) const
{
    // Check dimension bounds first - out-of-bounds areas show boundary terrain
    if( get_mapbuffer().is_outside_pocket_dimension_bounds( map_local_to_abs( *this, p ) ) ) {
        return get_mapbuffer().get_boundary_terrain();
    }

    point_sm_ms l;
    submap *const current_submap = get_submap_at( tripoint_bub_ms( p ), l );
    if( current_submap == nullptr ) {
        return t_null;
    }

    return current_submap->get_ter( l );
}

data_vars::data_set *map::ter_vars( const tripoint_bub_ms &p ) const
{
    if( !inbounds( p ) ) {
        return nullptr;
    }

    point_sm_ms l;
    const auto sm = get_submap_at( tripoint_bub_ms( p ), l );
    return &sm->get_ter_vars( l );
}


data_vars::data_set *map::furn_vars( const tripoint_bub_ms &p ) const
{
    if( !inbounds( p ) ) {
        return nullptr;
    }

    point_sm_ms l;
    const auto sm = get_submap_at( tripoint_bub_ms( p ), l );
    return &sm->get_furn_vars( l );
}

uint8_t map::get_known_connections( const tripoint_bub_ms &p, int connect_group,
                                    const std::map<tripoint_bub_ms, ter_id> &override ) const
{
    auto &ch = access_cache( p.z() );
    if( !ch.inbounds( p.xy() ) ) {
        return 0;
    }
    uint8_t val = 0;
    std::function<bool( const tripoint_bub_ms & )> is_memorized;
#ifdef TILES
    if( use_tiles ) {
        is_memorized =
        [&]( const tripoint_bub_ms & q ) {
            return !g->u.get_memorized_tile( map_local_to_abs( *this, q ) ).tile.empty();
        };
    } else {
#endif
        is_memorized =
        [&]( const tripoint_bub_ms & q ) {
            return g->u.get_memorized_symbol( map_local_to_abs( *this, q ) );
        };
#ifdef TILES
    }
#endif

    const bool overridden = override.contains( p );
    const bool is_transparent = ch.transparency_cache[ch.idx( p.x(),
                                        p.y() )] > LIGHT_TRANSPARENCY_SOLID;

    // populate connection information
    for( int i = 0; i < 4; ++i ) {
        auto neighbour = p + offsets[i];
        if( !inbounds( neighbour ) ) {
            continue;
        }
        const auto neighbour_override = override.find( neighbour );
        const bool neighbour_overridden = neighbour_override != override.end();
        // if there's some non-memory terrain to show at the neighboring tile
        const bool may_connect = neighbour_overridden ||
                                 get_visibility( ch.visibility_cache[ch.idx( neighbour.x(), neighbour.y() )],
                                         get_visibility_variables_cache() ) == VIS_CLEAR ||
                                 // or if an actual center tile is transparent or next to a memorized tile
                                 ( !overridden && ( is_transparent || is_memorized( neighbour ) ) );
        if( may_connect ) {
            const ter_t &neighbour_terrain = neighbour_overridden ?
                                             neighbour_override->second.obj() : ter( neighbour ).obj();
            if( neighbour_terrain.connects_to( connect_group ) ) {
                val += 1 << i;
            }
        }
    }

    return val;
}


uint8_t map::get_known_connections_f( const tripoint_bub_ms &p, int connect_group,
                                      const std::map<tripoint_bub_ms, furn_id> &override ) const
{
    const level_cache &ch = access_cache( p.z() );
    if( !ch.inbounds( p.xy() ) ) {
        return 0;
    }
    uint8_t val = 0;
    std::function<bool( const tripoint_bub_ms & )> is_memorized;
    avatar &player_character = get_avatar();
#ifdef TILES
    if( use_tiles ) {
        is_memorized = [&]( const tripoint_bub_ms & q ) {
            return !player_character.get_memorized_tile( map_local_to_abs( *this, q ) ).tile.empty();
        };
    } else {
#endif
        is_memorized = [&]( const tripoint_bub_ms & q ) {
            return player_character.get_memorized_symbol( map_local_to_abs( *this, q ) );
        };
#ifdef TILES
    }
#endif

    const bool overridden = override.contains( p );
    const bool is_transparent = ch.transparency_cache[ch.idx( p.x(),
                                        p.y() )] > LIGHT_TRANSPARENCY_SOLID;

    // populate connection information
    for( int i = 0; i < 4; ++i ) {
        auto pt = p + offsets[i];
        if( !inbounds( pt ) ) {
            continue;
        }
        const auto neighbour_override = override.find( pt );
        const bool neighbour_overridden = neighbour_override != override.end();
        // if there's some non-memory terrain to show at the neighboring tile
        const bool may_connect = neighbour_overridden ||
                                 get_visibility( ch.visibility_cache[ch.idx( pt.x(), pt.y() )],
                                         get_visibility_variables_cache() ) ==
                                 visibility_type::VIS_CLEAR ||
                                 // or if an actual center tile is transparent or
                                 // next to a memorized tile
                                 ( !overridden && ( is_transparent || is_memorized( pt ) ) );
        if( may_connect ) {
            const furn_t &neighbour_furn = neighbour_overridden ?
                                           neighbour_override->second.obj() : furn( pt ).obj();
            if( neighbour_furn.connects_to( connect_group ) ) {
                val += 1 << i;
            }
        }
    }

    return val;
}


/*
 * Get the results of harvesting this tile's furniture or terrain
 */
const harvest_id &map::get_harvest( const tripoint_bub_ms &pos ) const
{
    const auto furn_here = furn( pos );
    if( furn_here->examine != iexamine::none ) {
        // Note: if furniture can be examined, the terrain can NOT (until furniture is removed)
        if( furn_here->has_flag( TFLAG_HARVESTED ) ) {
            return harvest_id::NULL_ID();
        }

        return furn_here->get_harvest();
    }

    const auto ter_here = ter( pos );
    if( ter_here->has_flag( TFLAG_HARVESTED ) ) {
        return harvest_id::NULL_ID();
    }

    return ter_here->get_harvest();
}

const std::set<std::string> &map::get_harvest_names( const tripoint_bub_ms &pos ) const
{
    static const std::set<std::string> null_harvest_names = {};
    const auto furn_here = furn( pos );
    if( furn_here->examine != iexamine::none ) {
        if( furn_here->has_flag( TFLAG_HARVESTED ) ) {
            return null_harvest_names;
        }

        return furn_here->get_harvest_names();
    }

    const auto ter_here = ter( pos );
    if( ter_here->has_flag( TFLAG_HARVESTED ) ) {
        return null_harvest_names;
    }

    return ter_here->get_harvest_names();
}

/*
 * Get the terrain transforms_into id (what will the terrain transforms into)
 */
ter_id map::get_ter_transforms_into( const tripoint_bub_ms &p ) const
{
    return ter( p ).obj().transforms_into.id();
}

furn_id map::get_furn_transforms_into( const tripoint_bub_ms &p ) const
{
    return furn( p ).obj().transforms_into.id();
}
/**
 * Examines the tile pos, with character as the "examinator"
 * Casts Character to player because player/NPC split isn't done yet
 */
void map::examine( Character &who, const tripoint_bub_ms &pos )
{
    const auto furn_here = furn( pos ).obj();
    if( furn_here.examine != iexamine::none ) {
        furn_here.examine( dynamic_cast<player &>( who ), pos );
    } else {
        ter( pos ).obj().examine( dynamic_cast<player &>( who ), pos );
    }
}

bool map::is_harvestable( const tripoint_bub_ms &pos ) const
{
    const auto &harvest_here = get_harvest( pos );
    return !harvest_here.is_null() && !harvest_here->empty();
}

/*
 * set terrain via string; this works for -any- terrain id
 */
bool map::ter_set( const tripoint_bub_ms &p, const ter_id &new_terrain )
{
    const auto abs_pos = map_local_to_abs( *this, p );
    if( get_mapbuffer().is_outside_pocket_dimension_bounds( abs_pos ) ) {
        return false;
    }

    return get_mapbuffer().set_ter( abs_pos, new_terrain, resident_item_lookup() );
}

std::string map::tername( const tripoint_bub_ms &p ) const
{
    return ter( p ).obj().name();
}

std::string map::features( const tripoint_bub_ms &p )
{
    std::string result;
    const auto add = [&]( const std::string & text ) {
        if( !result.empty() ) {
            result += " ";
        }
        result += text;
    };
    const auto add_if = [&]( const bool cond, const std::string & text ) {
        if( cond ) {
            add( text );
        }
    };
    // This is used in an info window that is 46 characters wide, and is expected
    // to take up one line.  So, make sure it does that.
    // FIXME: can't control length of localized text.
    const auto &feature_descriptions = map_feature_descriptions::get_map_feature_descriptions();
    using map_feature_descriptions::map_feature_description;
    for( const auto &description : feature_descriptions ) {
        bool condition = false;
        switch( description.test ) {
            case map_feature_description::test_type::bashable:
                condition = is_bashable( p );
                break;
            case map_feature_description::test_type::diggable:
                condition = ter( p )->is_diggable();
                break;
            case map_feature_description::test_type::flag:
                condition = has_flag( description.flag, p );
                break;
        }
        add_if( condition, description.text.translated() );
    }
    return result;
}

int map::move_cost_internal( const furn_t &furniture, const ter_t &terrain, const vehicle *veh,
                             const int vpart ) const
{
    if( terrain.movecost == 0 || ( furniture.id && furniture.movecost < 0 ) ) {
        return 0;
    }

    if( veh != nullptr ) {
        const vpart_position vp( const_cast<vehicle &>( *veh ), vpart );
        if( vp.obstacle_at_part() ) {
            return 0;
        } else if( vp.part_with_feature( VPFLAG_AISLE, true ) ) {
            return 2;
        } else {
            return 8;
        }
    }

    if( furniture.id ) {
        return std::max( terrain.movecost + furniture.movecost, 0 );
    }

    return std::max( terrain.movecost, 0 );
}

bool map::is_wall_adjacent( const tripoint_bub_ms &center ) const
{
    for( const tripoint_bub_ms &p : points_in_radius( center, 1 ) ) {
        if( p != center && impassable( p ) ) {
            return true;
        }
    }
    return false;
}

// Move cost: 3D

int map::move_cost( const tripoint_bub_ms &p, const vehicle *ignored_vehicle ) const
{
    // Dimension bounds are always impassable
    if( get_mapbuffer().is_outside_pocket_dimension_bounds( map_local_to_abs( *this, p ) ) ) {
        return 0;
    }

    const furn_t &furniture = furn( p ).obj();
    const ter_t &terrain = ter( p ).obj();
    const optional_vpart_position vp = veh_at( p );
    vehicle *const veh = ( !vp || &vp->vehicle() == ignored_vehicle ) ? nullptr : &vp->vehicle();
    const int part = veh ? vp->part_index() : -1;

    return move_cost_internal( furniture, terrain, veh, part );
}

bool map::impassable( const tripoint_bub_ms &p ) const
{
    return !passable( p );
}

bool map::passable( const tripoint_bub_ms &p ) const
{
    return move_cost( p ) != 0;
}

int map::move_cost_ter_furn( const tripoint_bub_ms &p ) const
{
    point_sm_ms l;
    submap *const current_submap = get_submap_at( tripoint_bub_ms( p ), l );
    if( current_submap == nullptr ) {
        return 0;
    }

    const int tercost = current_submap->get_ter( l ).obj().movecost;
    if( tercost == 0 ) {
        return 0;
    }

    const int furncost = current_submap->get_furn( l ).obj().movecost;
    if( furncost < 0 ) {
        return 0;
    }

    const int cost = tercost + furncost;
    return cost > 0 ? cost : 0;
}

bool map::impassable_ter_furn( const tripoint_bub_ms &p ) const
{
    return !passable_ter_furn( p );
}

bool map::passable_ter_furn( const tripoint_bub_ms &p ) const
{
    return move_cost_ter_furn( p ) != 0;
}

int map::combined_movecost( const tripoint_bub_ms &from, const tripoint_bub_ms &to,
                            const vehicle *ignored_vehicle,
                            const int modifier, const bool flying, const bool via_ramp ) const
{
    const int mults[4] = { 0, 50, 71, 100 };
    const int cost1 = move_cost( from, ignored_vehicle );
    const int cost2 = move_cost( to, ignored_vehicle );
    // Multiply cost depending on the number of differing axes
    // 0 if all axes are equal, 100% if only 1 differs, 141% for 2, 200% for 3
    size_t match = trigdist ? ( from.x() != to.x() ) + ( from.y() != to.y() ) +
                   ( from.z() != to.z() ) : 1;
    if( flying || from.z() == to.z() ) {
        return ( cost1 + cost2 + modifier ) * mults[match] / 2;
    }

    // Inter-z-level movement by foot (not flying)
    if( !valid_move( from, to, false, via_ramp ) ) {
        return 0;
    }

    // TODO: Penalize for using stairs
    return ( cost1 + cost2 + modifier ) * mults[match] / 2;
}

bool map::valid_move( const tripoint_bub_ms &from, const tripoint_bub_ms &to,
                      const bool bash, const bool flying, const bool via_ramp ) const
{
    return get_mapbuffer().valid_move(
    map_local_to_abs( *this, from ), map_local_to_abs( *this, to ), {
        .bash = bash,
        .flying = flying,
        .via_ramp = via_ramp
    } );
}

// End of move cost

double map::ranged_target_size( const tripoint_bub_ms &p ) const
{
    if( impassable( p ) ) {
        return 1.0;
    }

    if( !has_floor( p ) ) {
        return 0.0;
    }

    // TODO: Handle cases like shrubs, trees, furniture, sandbags...
    return 0.1;
}

int map::climb_difficulty( const tripoint_bub_ms &p ) const
{
    if( p.z() > OVERMAP_HEIGHT || p.z() < -OVERMAP_DEPTH ) {
        debugmsg( "climb_difficulty on out of bounds point: %d, %d, %d", p.x(), p.y(), p.z() );
        return INT_MAX;
    }

    return MAPBUFFER_REGISTRY.get( bound_dimension_ ).climb_difficulty(
               map_local_to_abs( *this, p ) ).value_or( INT_MAX );
}

bool map::has_floor( const tripoint_bub_ms &p, bool visible_only ) const
{
    if( p.z() < -OVERMAP_DEPTH || p.z() > OVERMAP_HEIGHT ) {
        return false;
    }

    point_sm_ms l;
    submap *sm = get_submap_at( tripoint_bub_ms( p ), l );
    if( !sm ) {
        return false;
    }
    if( sm->floor_dirty ) {
        const int smx = divide_round_to_minus_infinity( p.x(), SEEX );
        const int smy = divide_round_to_minus_infinity( p.y(), SEEY );
        sm->rebuild_floor_cache( *this, tripoint_bub_sm( smx, smy, p.z() ) );
    }
    return sm->floor_cache[l.x()][l.y()] || ( !visible_only && has_flag( TFLAG_Z_TRANSPARENT, p ) );
}

bool map::floor_between( const tripoint_bub_ms &first, const tripoint_bub_ms &second ) const
{
    int diff = std::abs( first.z() - second.z() );
    if( diff == 0 ) { //There's never a floor between two tiles on the same level
        return false;
    }
    if( diff != 1 ) {
        debugmsg( "map::floor_between should only be called on tiles that are exactly 1 z level apart" );
        return true;
    }
    int upper = std::max( first.z(), second.z() );
    if( first.xy() == second.xy() ) {
        return has_floor( tripoint_bub_ms( first.xy(), upper ) );
    }
    return has_floor( tripoint_bub_ms( first.xy(), upper ) ) &&
           has_floor( tripoint_bub_ms( second.xy(), upper ) );
}

bool map::supports_above( const tripoint_bub_ms &p ) const
{
    const maptile tile = maptile_at( tripoint_bub_ms( p ) );
    const ter_t &ter = tile.get_ter_t();
    if( ter.movecost == 0 ) {
        return true;
    }

    const furn_id frn_id = tile.get_furn();
    if( frn_id != f_null ) {
        const furn_t &frn = frn_id.obj();
        if( frn.movecost < 0 ) {
            return true;
        }
    }

    return veh_at( p ).has_value();
}

bool map::has_floor_or_support( const tripoint_bub_ms &p ) const
{
    if( p.z() < -OVERMAP_DEPTH || p.z() > OVERMAP_HEIGHT ) {
        return false;
    }
    if( has_floor( p ) ) {
        return true;
    }
    if( p.z() <= -OVERMAP_DEPTH ) {
        return false;
    }
    return supports_above( tripoint_bub_ms( p.xy(), p.z() - 1 ) );
}

void map::drop_everything( const tripoint_bub_ms &p )
{
    //Do a suspension check so that there won't be a floor there for the rest of this check.
    if( has_flag( "SUSPENDED", p ) ) {
        collapse_invalid_suspension( p );
    }
    if( has_floor( p ) ) {
        return;
    }

    drop_furniture( p );
    drop_items( p );
    drop_vehicle( p );
    drop_fields( p );
}

void map::drop_furniture( const tripoint_bub_ms &p )
{
    const furn_id frn = furn( p );
    if( frn == f_null ) {
        return;
    }

    enum support_state {
        SS_NO_SUPPORT = 0,
        SS_BAD_SUPPORT, // TODO: Implement bad, shaky support
        SS_GOOD_SUPPORT,
        SS_FLOOR, // Like good support, but bash floor instead of tile below
        SS_CREATURE
    };

    // Checks if the tile:
    // has floor (supports unconditionally)
    // has support below
    // has unsupporting furniture below (bad support, things should "slide" if possible)
    // has no support and thus allows things to fall through
    const auto check_tile = [this]( const tripoint_bub_ms & pt ) {
        if( has_floor( pt ) ) {
            return SS_FLOOR;
        }

        tripoint_bub_ms below_dest( pt.xy(), pt.z() - 1 );
        if( supports_above( below_dest ) ) {
            return SS_GOOD_SUPPORT;
        }

        const furn_id frn_id = furn( below_dest );
        if( frn_id != f_null ) {
            const furn_t &frn = frn_id.obj();
            // Allow crushing tiny/nocollide furniture
            if( !frn.has_flag( "TINY" ) && !frn.has_flag( "NOCOLLIDE" ) ) {
                return SS_BAD_SUPPORT;
            }
        }

        if( g->critter_at( below_dest ) != nullptr ) {
            // Smash a critter
            return SS_CREATURE;
        }

        return SS_NO_SUPPORT;
    };

    tripoint_bub_ms current( p.xy(), p.z() + 1 );
    support_state last_state = SS_NO_SUPPORT;
    while( last_state == SS_NO_SUPPORT && current.z() > -OVERMAP_DEPTH ) {
        current.z()--;
        // Check current tile
        last_state = check_tile( current );
    }

    if( current == p ) {
        // Nothing happened
        if( last_state != SS_FLOOR ) {
            support_dirty( current );
        }

        return;
    }

    furn_set( p, f_null );
    furn_set( current, frn );

    // If it's sealed, we need to drop items with it
    const auto &frn_obj = frn.obj();
    if( frn_obj.has_flag( TFLAG_SEALED ) && has_items( p ) ) {
        auto old_items = i_at( p );
        auto new_items = i_at( current );

        old_items.move_all_to( &new_items );
    }

    // Approximate weight/"bulkiness" based on strength to drag
    int weight;
    if( frn_obj.has_flag( "TINY" ) || frn_obj.has_flag( "NOCOLLIDE" ) ) {
        weight = 5;
    } else {
        weight = frn_obj.is_movable() ? frn_obj.move_str_req : 20;
    }

    if( frn_obj.has_flag( "ROUGH" ) || frn_obj.has_flag( "SHARP" ) ) {
        weight += 5;
    }

    // TODO: Balance this.
    int dmg = weight * ( p.z() - current.z() );

    if( last_state == SS_FLOOR ) {
        // Bash the same tile twice - once for furniture, once for the floor
        bash( current, dmg, false, false, true );
        bash( current, dmg, false, false, true );
    } else if( last_state == SS_BAD_SUPPORT || last_state == SS_GOOD_SUPPORT ) {
        bash( current, dmg, false, false, false );
        tripoint_bub_ms below( current.xy(), current.z() - 1 );
        bash( below, dmg, false, false, false );
    } else if( last_state == SS_CREATURE ) {
        const std::string &furn_name = frn_obj.name();
        bash( current, dmg, false, false, false );
        tripoint_bub_ms below( current.xy(), current.z() - 1 );
        Creature *critter = g->critter_at( below );
        if( critter == nullptr ) {
            debugmsg( "drop_furniture couldn't find creature at %d,%d,%d",
                      below.x(), below.y(), below.z() );
            return;
        }

        critter->add_msg_player_or_npc( m_bad, _( "Falling %s hits you!" ),
                                        _( "Falling %s hits <npcname>" ),
                                        furn_name );
        // TODO: A chance to dodge/uncanny dodge
        player *pl = dynamic_cast<player *>( critter );
        monster *mon = dynamic_cast<monster *>( critter );
        if( pl != nullptr ) {
            pl->deal_damage( nullptr, bodypart_id( "torso" ), damage_instance( DT_BASH, rng( dmg / 3, dmg ), 0,
                             0.5f ) );
            pl->deal_damage( nullptr, bodypart_id( "head" ),  damage_instance( DT_BASH, rng( dmg / 3, dmg ), 0,
                             0.5f ) );
            pl->deal_damage( nullptr, bodypart_id( "leg_l" ), damage_instance( DT_BASH, rng( dmg / 2, dmg ), 0,
                             0.4f ) );
            pl->deal_damage( nullptr, bodypart_id( "leg_r" ), damage_instance( DT_BASH, rng( dmg / 2, dmg ), 0,
                             0.4f ) );
            pl->deal_damage( nullptr, bodypart_id( "arm_l" ), damage_instance( DT_BASH, rng( dmg / 2, dmg ), 0,
                             0.4f ) );
            pl->deal_damage( nullptr, bodypart_id( "arm_r" ), damage_instance( DT_BASH, rng( dmg / 2, dmg ), 0,
                             0.4f ) );
        } else if( mon != nullptr ) {
            // TODO: Monster's armor and size - don't crush hulks with chairs
            mon->apply_damage( nullptr, bodypart_id( "torso" ), rng( dmg, dmg * 2 ) );
        }
    }

    // Re-queue for another check, in case bash destroyed something
    support_dirty( current );
}

void map::drop_items( const tripoint_bub_ms &p )
{
    if( !has_items( p ) ) {
        return;
    }

    auto items = i_at( p );
    // TODO: Make items check the volume tile below can accept
    // rather than disappearing if it would be overloaded

    tripoint_bub_ms below( p );
    while( below.z() >= -OVERMAP_DEPTH && !has_floor( below ) ) {
        below.z()--;
    }

    if( below == p ) {
        return;
    }
    map_stack stack = i_at( below );
    items.move_all_to( &stack );

    // TODO: Bash the item up before adding it
    // TODO: Bash the creature, terrain, furniture and vehicles on the tile
    // Just to make a sound for now
    bash( below, 1 );
    i_clear( p );
}

void map::drop_vehicle( const tripoint_bub_ms &p )
{
    const optional_vpart_position vp = veh_at( p );
    if( !vp ) {
        return;
    }

    vp->vehicle().is_falling = true;
}

void map::drop_fields( const tripoint_bub_ms &p )
{
    field &fld = field_at( p );
    if( fld.field_count() == 0 ) {
        return;
    }

    std::list<field_type_id> dropped;
    const tripoint_bub_ms below = p + tripoint_below;
    for( const auto &iter : fld ) {
        const field_entry &entry = iter.second;
        // For now only drop cosmetic fields, which don't warrant per-turn check
        // Active fields "drop themselves"
        if( entry.decays_on_actualize() ) {
            add_field( below, entry.get_field_type(), entry.get_field_intensity(), entry.get_field_age() );
            dropped.push_back( entry.get_field_type() );
        }
    }

    for( const auto &entry : dropped ) {
        fld.remove_field( entry );
    }
}

void map::support_dirty( const tripoint_bub_ms &p )
{
    support_cache_dirty.insert( p );
}

void map::process_falling()
{
    ZoneScoped;

    if( !support_cache_dirty.empty() ) {
        add_msg( m_debug, "Checking %d tiles for falling objects",
                 support_cache_dirty.size() );
        // We want the cache to stay constant, but falling can change it
        std::set<tripoint_bub_ms> last_cache = std::move( support_cache_dirty );
        support_cache_dirty.clear();
        for( const tripoint_bub_ms &p : last_cache ) {
            drop_everything( p );
        }
    }
}

bool map::has_flag( const std::string &flag, const tripoint_bub_ms &p ) const
{
    return has_flag_ter_or_furn( flag, p ); // Does bound checking
}

bool map::can_put_items( const tripoint_bub_ms &p ) const
{
    if( can_put_items_ter_furn( p ) ) {
        return true;
    }
    const optional_vpart_position vp = veh_at( p );
    return static_cast<bool>( vp.part_with_feature( "CARGO", true ) );
}

bool map::can_put_items_ter_furn( const tripoint_bub_ms &p ) const
{
    return !has_flag( "NOITEM", p ) && !has_flag( "SEALED", p );
}

bool map::has_flag_ter( const std::string &flag, const tripoint_bub_ms &p ) const
{
    return ter( p ).obj().has_flag( flag );
}

bool map::has_flag_furn( const std::string &flag, const tripoint_bub_ms &p ) const
{
    return furn( p ).obj().has_flag( flag );
}

bool map::has_flag_ter_or_furn( const std::string &flag, const tripoint_bub_ms &p ) const
{
    point_sm_ms l;
    submap *const current_submap = get_submap_at( tripoint_bub_ms( p ), l );

    return current_submap && //FIXME: can be null during mapgen
           ( current_submap->get_ter( l ).obj().has_flag( flag ) ||
             current_submap->get_furn( l ).obj().has_flag( flag ) );
}

bool map::has_flag( const ter_bitflags flag, const tripoint_bub_ms &p ) const
{
    return has_flag_ter_or_furn( flag, p ); // Does bound checking
}

bool map::has_flag_ter( const ter_bitflags flag, const tripoint_bub_ms &p ) const
{
    return ter( p ).obj().has_flag( flag );
}

bool map::has_flag_furn( const ter_bitflags flag, const tripoint_bub_ms &p ) const
{
    return furn( p ).obj().has_flag( flag );
}

bool map::has_flag_vpart( const std::string &flag, const tripoint_bub_ms &p ) const
{
    const optional_vpart_position vp = veh_at( p );
    return static_cast<bool>( vp.part_with_feature( flag, true ) );
}

bool map::has_flag_furn_or_vpart( const std::string &flag, const tripoint_bub_ms &p ) const
{
    return has_flag_furn( flag, p ) || has_flag_vpart( flag, p );
}

bool map::has_flag_ter_or_furn( const ter_bitflags flag, const tripoint_bub_ms &p ) const
{
    point_sm_ms l;
    submap *const current_submap = get_submap_at( tripoint_bub_ms( p ), l );

    return current_submap && //FIXME: can be null during mapgen
           ( current_submap->get_ter( l ).obj().has_flag( flag ) ||
             current_submap->get_furn( l ).obj().has_flag( flag ) );
}

// End of 3D flags

// Bashable - common function

int map::bash_rating_internal( const int str, const furn_t &furniture,
                               const ter_t &terrain, const bool allow_floor,
                               const vehicle *veh, const int part ) const
{
    bool furn_smash = false;
    bool ter_smash = false;
    ///\EFFECT_STR determines what furniture can be smashed
    if( furniture.id && furniture.bash.str_max != -1 ) {
        furn_smash = true;
        ///\EFFECT_STR determines what terrain can be smashed
    } else if( terrain.bash.str_max != -1 && ( !terrain.bash.bash_below || allow_floor ) ) {
        ter_smash = true;
    }

    if( veh != nullptr && vpart_position( const_cast<vehicle &>( *veh ), part ).obstacle_at_part() ) {
        // Monsters only care about rating > 0, NPCs should want to path around cars instead
        return 2; // Should probably be a function of part hp (+armor on tile)
    }

    int bash_min = 0;
    int bash_max = 0;
    if( furn_smash ) {
        bash_min = furniture.bash.str_min;
        bash_max = furniture.bash.str_max;
    } else if( ter_smash ) {
        bash_min = terrain.bash.str_min;
        bash_max = terrain.bash.str_max;
    } else {
        return -1;
    }

    ///\EFFECT_STR increases smashing damage
    if( str < bash_min ) {
        return 0;
    } else if( str >= bash_max ) {
        return 10;
    }

    int ret = ( 10 * ( str - bash_min ) ) / ( bash_max - bash_min );
    // Round up to 1, so that desperate NPCs can try to bash down walls
    return std::max( ret, 1 );
}

// 3D bashable

bool map::is_bashable( const tripoint_bub_ms &p, const bool allow_floor ) const
{
    if( veh_at( p ).obstacle_at_part() ) {
        return true;
    }

    if( has_furn( p ) && furn( p ).obj().bash.str_max != -1 ) {
        return true;
    }

    const auto &ter_bash = ter( p ).obj().bash;
    return ter_bash.str_max != -1 && ( !ter_bash.bash_below || allow_floor );
}

bool map::is_bashable_ter( const tripoint_bub_ms &p, const bool allow_floor ) const
{
    const auto &ter_bash = ter( p ).obj().bash;
    return ter_bash.str_max != -1 && ( ( !ter_bash.bash_below &&
                                         !ter( p ).obj().has_flag( "VEH_TREAT_AS_BASH_BELOW" ) ) || allow_floor );
}

bool map::is_bashable_furn( const tripoint_bub_ms &p ) const
{
    return has_furn( p ) && furn( p ).obj().bash.str_max != -1;
}

bool map::is_bashable_ter_furn( const tripoint_bub_ms &p, const bool allow_floor ) const
{
    return is_bashable_furn( p ) || is_bashable_ter( p, allow_floor );
}

int map::bash_strength( const tripoint_bub_ms &p, const bool allow_floor ) const
{
    if( has_furn( p ) && furn( p ).obj().bash.str_max != -1 ) {
        return furn( p ).obj().bash.str_max;
    }

    const auto &ter_bash = ter( p ).obj().bash;
    if( ter_bash.str_max != -1 && ( !ter_bash.bash_below || allow_floor ) ) {
        return ter_bash.str_max;
    }

    return -1;
}

int map::bash_resistance( const tripoint_bub_ms &p, const bool allow_floor ) const
{
    if( has_furn( p ) && furn( p ).obj().bash.str_min != -1 ) {
        return furn( p ).obj().bash.str_min;
    }

    const auto &ter_bash = ter( p ).obj().bash;
    if( ter_bash.str_min != -1 && ( !ter_bash.bash_below || allow_floor ) ) {
        return ter_bash.str_min;
    }

    return -1;
}

int map::bash_rating( const int str, const tripoint_bub_ms &p, const bool allow_floor ) const
{
    if( str <= 0 ) {
        return -1;
    }

    const furn_t &furniture = furn( p ).obj();
    const ter_t &terrain = ter( p ).obj();
    const optional_vpart_position vp = veh_at( p );
    vehicle *const veh = vp ? &vp->vehicle() : nullptr;
    const int part = vp ? vp->part_index() : -1;
    return bash_rating_internal( str, furniture, terrain, allow_floor, veh, part );
}

// End of 3D bashable

void map::make_rubble( const tripoint_bub_ms &p, const furn_id &rubble_type,
                       const ter_id &floor_type, bool overwrite )
{
    if( overwrite ) {
        ter_set( p, floor_type );
        furn_set( p, rubble_type );
    } else {
        // First see if there is existing furniture to destroy
        if( is_bashable_furn( p ) ) {
            destroy_furn( p, true );
        }
        // Leave the terrain alone unless it interferes with furniture placement
        if( impassable( p ) && is_bashable_ter( p ) ) {
            destroy( p, true );
        }
        // Check again for new terrain after potential destruction
        if( impassable( p ) ) {
            ter_set( p, floor_type );
        }

        furn_set( p, rubble_type );
    }
}

bool map::is_water_shallow_current( const tripoint_bub_ms &p ) const
{
    return has_flag( "CURRENT", p ) && !has_flag( TFLAG_DEEP_WATER, p );
}

bool map::is_divable( const tripoint_bub_ms &p ) const
{
    const std::optional<vpart_reference> vp = veh_at( p ).part_with_feature( VPFLAG_BOARDABLE,
            true );
    if( !vp ) {
        return has_flag( "SWIMMABLE", p ) && has_flag( TFLAG_DEEP_WATER, p );
    }
    return false;
}

bool map::is_outside( const tripoint_bub_ms &p ) const
{
    point_sm_ms l;
    submap *sm = get_submap_at( tripoint_bub_ms( p ), l );
    if( !sm ) {
        return true;
    }
    if( sm->outside_dirty ) {
        const int smx = divide_round_to_minus_infinity( p.x(), SEEX );
        const int smy = divide_round_to_minus_infinity( p.y(), SEEY );
        const level_cache *above = ( p.z() < OVERMAP_HEIGHT )
                                   ? &get_cache_ref( p.z() + 1 )
                                   : nullptr;
        sm->rebuild_outside_cache( above, tripoint_bub_sm( smx, smy, p.z() ) );
    }
    return sm->outside_cache[l.x()][l.y()];
}

bool map::is_sheltered( const tripoint_bub_ms &p ) const
{
    point_sm_ms l;
    submap *sm = get_submap_at( p, l );
    if( !sm ) {
        return true; // outside loaded area — treat as sheltered
    }
    if( sm->outside_dirty ) {
        const level_cache *above = ( p.z() < OVERMAP_HEIGHT )
                                   ? &get_cache_ref( p.z() + 1 )
                                   : nullptr;
        sm->rebuild_outside_cache( above, project_to<coords::sm>( p ) );
    }
    return sm->sheltered_cache[l.x()][l.y()];
}

float map::get_transparency( const tripoint_bub_ms &p ) const
{
    point_sm_ms l;
    submap *sm = get_submap_at( p, l );
    if( !sm ) {
        return LIGHT_TRANSPARENCY_SOLID;
    }
    if( sm->transparency_dirty ) {
        sm->rebuild_transparency_cache( *this, project_to<coords::sm>( p ) );
    }
    return sm->transparency_cache[l.x()][l.y()];
}

bool map::is_last_ter_wall( const bool no_furn, const tripoint_bub_ms &p,
                            const tripoint_bub_ms &max, const direction dir ) const
{
    tripoint_rel_ms mov;
    switch( dir ) {
        case direction::NORTH:
            mov = tripoint_rel_ms::north();
            break;
        case direction::SOUTH:
            mov = tripoint_rel_ms::south();
            break;
        case direction::WEST:
            mov = tripoint_rel_ms::west();
            break;
        case direction::EAST:
            mov = tripoint_rel_ms::east();
            break;
        default:
            return false;
    }
    auto p2( p );
    bool result = true;
    bool loop = true;
    while( ( loop ) && ( ( dir == direction::NORTH && p2.y() >= 0 ) ||
                         ( dir == direction::SOUTH && p2.y() < max.y() ) ||
                         ( dir == direction::WEST  && p2.x() >= 0 ) ||
                         ( dir == direction::EAST  && p2.x() < max.x() ) ) ) {
        if( no_furn && has_furn( p2 ) ) {
            loop = false;
            result = false;
        } else if( !has_flag_ter( "FLAT", p2 ) ) {
            loop = false;
            if( !has_flag_ter( "WALL", p2 ) ) {
                result = false;
            }
        }
        p2.x() += mov.x();
        p2.y() += mov.y();
    }
    return result;
}

bool map::tinder_at( const tripoint_bub_ms &p )
{
    for( const auto &i : i_at( p ) ) {
        if( i->has_flag( flag_TINDER ) ) {
            return true;
        }
    }
    return false;
}

bool map::flammable_items_at( const tripoint_bub_ms &p, int threshold )
{
    if( !has_items( p ) ||
        ( has_flag( TFLAG_SEALED, p ) && !has_flag( TFLAG_ALLOW_FIELD_EFFECT, p ) ) ) {
        // Sealed containers don't allow fire, so shouldn't allow setting the fire either
        return false;
    }

    for( const auto &i : i_at( p ) ) {
        if( i->flammable( threshold ) ) {
            return true;
        }
    }

    return false;
}

bool map::is_flammable( const tripoint_bub_ms &p )
{
    if( flammable_items_at( p ) ) {
        return true;
    }

    if( has_flag( "FLAMMABLE", p ) ) {
        return true;
    }

    if( has_flag( "FLAMMABLE_ASH", p ) ) {
        return true;
    }

    if( get_field_intensity( p, fd_web ) > 0 ) {
        return true;
    }

    return false;
}

void map::decay_fields_and_scent( const time_duration &amount )
{
    ZoneScopedN( "decay_fields_and_scent" );
    // TODO: Make this happen on all z-levels

    // Decay scent separately, so that later we can use field count to skip empty submaps
    g->scent.decay();

    // Coordinate code copied from lightmap calculations
    // TODO: Z
    for( const auto p : bubble_submaps() ) {
        level_cache &smz_cache = get_cache( p.z() );
        const auto abs_sm = map_local_to_abs( *this, p );
        const auto cur_submap = get_mapbuffer().lookup_submap_in_memory( abs_sm );
        if( cur_submap == nullptr ) {
            continue;
        }
        int to_proc = cur_submap->field_count;
        if( to_proc < 1 ) {
            if( to_proc < 0 ) {
                cur_submap->field_count = 0;
                dbg( DL::Error ) << "map::decay_fields_and_scent: submap at "
                                 << abs_sm
                                 << "has " << to_proc << " field_count";
            }
            // This submap has no fields
            continue;
        }

        if( to_proc > 0 ) {
            for( const auto sm_ms : submap_tiles() ) {
                const auto ms_pos = project_combine( p, sm_ms );

                field &fields = cur_submap->get_field( sm_ms );
                if( !smz_cache.outside_cache[smz_cache.idx( ms_pos.x(), ms_pos.y() )] ) {
                    to_proc -= fields.field_count();
                } else {
                    for( auto &fp : fields ) {
                        to_proc--;
                        field_entry &cur = fp.second;
                        const field_type_id type = cur.get_field_type();
                        const int decay_amount_factor =  type.obj().decay_amount_factor;
                        if( decay_amount_factor != 0 ) {
                            const time_duration decay_amount = amount / decay_amount_factor;
                            cur.set_field_age( cur.get_field_age() + decay_amount );
                        }
                    }
                }
            }
        }

        if( to_proc > 0 ) {
            cur_submap->field_count = cur_submap->field_count - to_proc;
            dbg( DL::Warn ) << "map::decay_fields_and_scent: submap at "
                            << abs_sm
                            << "has " << cur_submap->field_count - to_proc << "fields, but "
                            << cur_submap->field_count << " field_count";
        }
    }
}

tripoint_bub_ms map::random_outdoor_tile()
{
    std::vector<tripoint_bub_ms> options;
    for( const tripoint_bub_ms &p : points_on_zlevel() ) {
        if( is_outside( p ) ) {
            options.push_back( p );
        }
    }
    return random_entry( options, tripoint_bub_ms::north_west() );
}

bool map::has_item_with( const tripoint_bub_ms &p,
                         const std::function<bool( const item & )> &filter )
{
    for( const item *it : i_at( p ) ) {
        if( filter( *it ) ) {
            return true;
        }
    }
    return false;
}

bool map::has_adjacent_item_with( const tripoint_bub_ms &p,
                                  const std::function<bool( const item & )> &filter )
{
    for( const auto &adj : points_in_radius( p, 1 ) ) {
        if( !has_items( adj ) ) {
            continue;
        }

        for( const item *it : i_at( adj ) ) {
            if( filter( *it ) ) {
                return true;
            }
        }
    }
    return false;
}

bool map::has_adjacent_furniture_with( const tripoint_bub_ms &p,
                                       const std::function<bool( const furn_t & )> &filter )
{
    for( const auto &adj : points_in_radius( p, 1 ) ) {
        if( has_furn( adj ) && filter( furn( adj ).obj() ) ) {
            return true;
        }
    }

    return false;
}

bool map::has_adjacent_terrain_with( const tripoint_bub_ms &p,
                                     const std::function<bool( const ter_t & )> &filter )
{
    for( const auto &adj : points_in_radius( p, 1 ) ) {
        if( filter( ter( adj ).obj() ) ) {
            return true;
        }
    }

    return false;
}

bool map::has_nearby( const tripoint_bub_ms &p,
                      const std::function<bool( map &m, const tripoint_bub_ms &p )> &pred, int radius )
{
    for( const tripoint_bub_ms &adj : points_in_radius( p, radius ) ) {
        if( pred( *this, adj ) ) {
            return true;
        }
    }
    return false;
}

bool map::has_nearby_fire( const tripoint_bub_ms &p, int radius )
{
    for( const tripoint_bub_ms &pt : points_in_radius( p, radius ) ) {
        if( get_field( pt, fd_fire ) != nullptr ) {
            return true;
        }
        if( has_flag_ter_or_furn( "USABLE_FIRE", pt ) ) {
            return true;
        }
    }
    return false;
}

bool map::has_nearby_table( const tripoint_bub_ms &p, int radius )
{
    for( const tripoint_bub_ms &pt : points_in_radius( p, radius ) ) {
        const optional_vpart_position vp = veh_at( p );
        if( has_flag( "FLAT_SURF", pt ) ) {
            return true;
        }
        if( vp && ( vp->vehicle().has_part( "FLAT_SURF" ) ) ) {
            return true;
        }
    }
    return false;
}

bool map::has_nearby_chair( const tripoint_bub_ms &p, int radius )
{
    for( const tripoint_bub_ms &pt : points_in_radius( p, radius ) ) {
        const optional_vpart_position vp = veh_at( pt );
        if( has_flag( "CAN_SIT", pt ) ) {
            return true;
        }
        if( vp && vp->vehicle().has_part( "SEAT" ) ) {
            return true;
        }
    }
    return false;
}

bool map::mop_spills( const tripoint_bub_ms &p )
{
    bool retval = false;

    if( !has_flag( "LIQUIDCONT", p ) && !has_flag( "SEALED", p ) ) {
        auto items = i_at( p );

        items.remove_top_items_with( [&retval]( detached_ptr<item> &&e ) {
            if( e->made_of( LIQUID ) ) {
                retval = true;
                return detached_ptr<item>();
            }
            return std::move( e );
        } );
    }

    field &fld = field_at( p );
    static const std::vector<field_type_id> to_check = {
        fd_blood,
        fd_blood_veggy,
        fd_blood_insect,
        fd_blood_invertebrate,
        fd_gibs_flesh,
        fd_gibs_veggy,
        fd_gibs_insect,
        fd_gibs_invertebrate,
        fd_bile,
        fd_slime,
        fd_sludge
    };
    for( field_type_id fid : to_check ) {
        retval |= fld.remove_field( fid );
    }

    if( const optional_vpart_position vp = veh_at( p ) ) {
        vehicle *const veh = &vp->vehicle();
        std::vector<int> parts_here = veh->parts_at_relative( vp->mount(), true );
        for( auto &elem : parts_here ) {
            if( veh->part( elem ).blood > 0 ) {
                veh->part( elem ).blood = 0;
                retval = true;
            }
            //remove any liquids that somehow didn't fall through to the ground
            vehicle_stack here = veh->get_items( elem );
            here.remove_top_items_with( [&retval]( detached_ptr<item> &&e ) {
                if( e->made_of( LIQUID ) ) {
                    retval = true;
                    return detached_ptr<item>();
                }
                return std::move( e );
            } );
        }
    } // if veh != 0
    return retval;
}

int map::collapse_check( const tripoint_bub_ms &p )
{
    const bool collapses = has_flag( TFLAG_COLLAPSES, p );
    const bool supports_roof = has_flag( TFLAG_SUPPORTS_ROOF, p );

    int num_supports = p.z() == -OVERMAP_DEPTH ? 0 : -5;
    // if there's support below, things are less likely to collapse
    if( p.z() > -OVERMAP_DEPTH ) {
        const auto &pbelow = tripoint_bub_ms( p.xy(), p.z() - 1 );
        for( const auto &tbelow : points_in_radius( pbelow, 1 ) ) {
            if( has_flag( TFLAG_SUPPORTS_ROOF, pbelow ) ) {
                num_supports += 1;
                if( has_flag( TFLAG_WALL, pbelow ) ) {
                    num_supports += 2;
                }
                if( tbelow == pbelow ) {
                    num_supports += 2;
                }
            }
        }
    }

    for( const auto &t : points_in_radius( p, 1 ) ) {
        if( p == t ) {
            continue;
        }

        if( collapses ) {
            if( has_flag( TFLAG_COLLAPSES, t ) ) {
                num_supports++;
            } else if( has_flag( TFLAG_SUPPORTS_ROOF, t ) ) {
                num_supports += 2;
            }
        } else if( supports_roof ) {
            if( has_flag( TFLAG_SUPPORTS_ROOF, t ) ) {
                if( has_flag( TFLAG_WALL, t ) ) {
                    num_supports += 4;
                } else if( !has_flag( TFLAG_COLLAPSES, t ) ) {
                    num_supports += 3;
                }
            }
        }
    }

    return 1.7 * num_supports;
}

// there is still some odd behavior here and there and you can get floating chunks of
// unsupported floor, but this is much better than it used to be
void map::collapse_at( const tripoint_bub_ms &p, const bool silent, const bool was_supporting,
                       const bool destroy_pos )
{
    const bool supports = was_supporting || has_flag( TFLAG_SUPPORTS_ROOF, p );
    const bool wall = was_supporting || has_flag( TFLAG_WALL, p );
    // don't bash again if the caller already bashed here
    if( destroy_pos ) {
        destroy( p, silent );
        crush( p );
        make_rubble( p );
    }
    const bool still_supports = has_flag( TFLAG_SUPPORTS_ROOF, p );

    // If something supporting the roof collapsed, see what else collapses
    if( supports && !still_supports ) {
        for( const auto &t : points_in_radius( p, 1 ) ) {
            // If z-levels are off, tz == t, so we end up skipping a lot of stuff to avoid bugs.
            const auto &tz = tripoint_bub_ms( t.xy(), t.z() + 1 );
            // if nothing above us had the chance of collapsing, move on
            if( !one_in( collapse_check( tz ) ) ) {
                continue;
            }
            // if a wall collapses, walls without support from below risk collapsing and
            //propagate the collapse upwards
            if( wall && p == t && has_flag( TFLAG_WALL, tz ) ) {
                collapse_at( tz, silent );
            }
            // floors without support from below risk collapsing into open air and can propagate
            // the collapse horizontally but not vertically
            if( p != t && ( has_flag( TFLAG_SUPPORTS_ROOF, t ) && has_flag( TFLAG_COLLAPSES, t ) ) ) {
                collapse_at( t, silent );
            }
        }
        // this tile used to support a roof, now it doesn't, which means there is only
        // open air above us
        const tripoint_bub_ms tabove( p.xy(), p.z() + 1 );
        ter_set( tabove, t_open_air );
        furn_set( tabove, f_null );
        propagate_suspension_check( tabove );
    }
    // it would be great to check if collapsing ceilings smashed through the floor, but
    // that's not handled for now
}

void map::propagate_suspension_check( const tripoint_bub_ms &point )
{
    for( const auto &neighbor : points_in_radius( point, 1 ) ) {
        if( neighbor != point && has_flag( TFLAG_SUSPENDED, neighbor ) ) {
            collapse_invalid_suspension( neighbor );
        }
    }
}

void map::collapse_invalid_suspension( const tripoint_bub_ms &point )
{
    if( !is_suspension_valid( point ) ) {
        ter_set( point, t_open_air );
        furn_set( point, f_null );

        propagate_suspension_check( point );
    }
}

bool map::is_suspension_valid( const tripoint_bub_ms &point )
{
    if( ter( point + tripoint_east ) != t_open_air
        && ter( point + tripoint_west ) != t_open_air ) {
        return true;
    }
    if( ter( point + tripoint_south_east ) != t_open_air
        && ter( point + tripoint_north_west ) != t_open_air ) {
        return true;
    }
    if( ter( point + tripoint_south ) != t_open_air
        && ter( point + tripoint_north ) != t_open_air ) {
        return true;
    }
    if( ter( point + tripoint_north_east ) != t_open_air
        && ter( point + tripoint_south_west ) != t_open_air ) {
        return true;
    }
    return false;
}

static bool will_explode_on_impact( const int power )
{
    const auto explode_threshold = get_option<int>( "MADE_OF_EXPLODIUM" );
    const bool is_explodium = explode_threshold != 0;

    return is_explodium && power >= explode_threshold;
}

void map::smash_trap( const tripoint_bub_ms &p, const int power, const std::string &cause_message )
{
    const trap &tr = get_map().tr_at( p );
    if( tr.is_null() ) {
        return;
    }

    const bool is_explosive_trap = !tr.is_benign() && tr.vehicle_data.do_explosion;

    if( !will_explode_on_impact( power ) || !is_explosive_trap ) {
        return;
    }
    // make a fake NPC to trigger the trap
    npc dummy;
    dummy.set_fake( true );
    dummy.name = cause_message;
    dummy.setpos( p );
    tr.trigger( p, &dummy );
}

void map::smash_items( const tripoint_bub_ms &p, const int power, const std::string &cause_message,
                       bool do_destroy )
{
    if( !has_items( p ) ) {
        return;
    }

    // Keep track of how many items have been damaged, and what the first one is
    int items_damaged = 0;
    int items_destroyed = 0;
    std::string damaged_item_name;

    // TODO: Bullets should be pretty much corpse-only
    constexpr const int min_destroy_threshold = 50;

    std::vector<detached_ptr<item>> contents;
    map_stack items = i_at( p );

    items.remove_top_items_with( [&]( detached_ptr<item> &&it ) {
        if( it->has_flag( flag_EXPLOSION_SMASHED ) ) {
            return std::move( it );
        }
        if( will_explode_on_impact( power ) && it->will_explode_in_fire() ) {
            return item::detonate( std::move( it ), p, contents );
        }
        if( it->is_corpse() ) {
            if( ( power < min_destroy_threshold || !do_destroy ) && !it->can_revive() &&
                !it->get_mtype()->zombify_into ) {
                return std::move( it );
            }
        }
        bool is_active_explosive = it->is_active() && it->type->get_use( "explosion" ) != nullptr;
        if( is_active_explosive && it->charges == 0 ) {
            return std::move( it );
        }

        const float material_factor = it->chip_resistance( true );
        // Intact non-rezing get a boost
        const float intact_mult = 2.0f -
                                  ( static_cast<float>( it->damage_level( it->max_damage() ) ) / it->max_damage() );
        const float destroy_threshold = min_destroy_threshold
                                        + material_factor * intact_mult;
        // For pulping, only consider material resistance. Non-rezing can only be destroyed.
        const float pulp_threshold = it->can_revive() ? material_factor : destroy_threshold;
        // Active explosives that will explode this turn are indestructible (they are exploding "now")
        if( power < pulp_threshold ) {
            return std::move( it );
        }

        bool item_was_destroyed = false;
        float destroy_chance = ( power - pulp_threshold ) / 4.0;

        const bool by_charges = it->count_by_charges();
        if( by_charges ) {
            destroy_chance *= it->charges_per_volume( 250_ml );
            if( x_in_y( destroy_chance, destroy_threshold ) ) {
                item_was_destroyed = true;
            }
        } else {
            const field_type_id type_blood = it->is_corpse() ? it->get_mtype()->bloodType() : fd_null;
            float roll = rng_float( 0.0, destroy_chance );
            if( roll >= destroy_threshold ) {
                item_was_destroyed = true;
            } else if( roll >= pulp_threshold ) {
                // Only pulp
                it->set_damage( it->max_damage() );
                // TODO: Blood streak cone away from explosion
                add_splash( type_blood, p, 1, destroy_chance );
                // If it was the first item to be damaged, note it
                if( items_damaged == 0 ) {
                    damaged_item_name = it->tname();
                }
                items_damaged++;
            }
        }
        if( item_was_destroyed ) {
            // But save the contents, except for irremovable gunmods
            it->contents.remove_top_items_with( [&contents]( detached_ptr<item> &&it ) {
                if( !it->is_irremovable() ) {
                    contents.push_back( std::move( it ) );
                    return detached_ptr<item>();
                } else {
                    return std::move( it );
                }
            } );
            if( items_damaged == 0 ) {
                damaged_item_name = it->tname();
            }

            items_damaged++;
            items_destroyed++;
            return detached_ptr<item>();
        } else {
            return std::move( it );
        }
    } );

    // Let the player know that the item was damaged if they can see it.
    if( items_destroyed > 1 && g->u.sees( p ) ) {
        add_msg( m_bad, _( "The %s destroys several items!" ), cause_message );
    } else if( items_destroyed == 1 && items_damaged == 1 && g->u.sees( p ) )  {
        //~ %1$s: the cause of destruction, %2$s: destroyed item name
        add_msg( m_bad, _( "The %1$s destroys the %2$s!" ), cause_message, damaged_item_name );
    } else if( items_damaged > 1 && g->u.sees( p ) ) {
        add_msg( m_bad, _( "The %s damages several items." ), cause_message );
    } else if( items_damaged == 1 && g->u.sees( p ) )  {
        //~ %1$s: the cause of damage, %2$s: damaged item name
        add_msg( m_bad, _( "The %1$s damages the %2$s." ), cause_message, damaged_item_name );
    }

    for( detached_ptr<item> &it : contents ) {
        add_item_or_charges( p, std::move( it ) );
    }
}

ter_id map::get_roof( const tripoint_bub_ms &p, const bool allow_air ) const
{
    if( p.z() <= -OVERMAP_DEPTH ) {
        // Could be magma/"void" instead
        return t_rock_floor;
    }

    const auto &ter_there = ter( p ).obj();
    const auto &roof = ter_there.roof;
    if( !roof ) {
        // No roof
        if( !allow_air ) {
            // TODO: Biomes? By setting? Forbid and treat as bug?
            if( p.z() < 0 ) {
                return t_rock_floor_no_roof;
            }

            return t_dirt;
        }

        return t_open_air;
    }

    ter_id new_ter = roof.id();
    if( new_ter == t_null ) {
        debugmsg( "map::get_new_floor: %d,%d,%d has invalid roof type %s",
                  p.x(), p.y(), p.z(), roof.c_str() );
        return t_dirt;
    }

    if( p.z() == -1 && new_ter == t_rock_floor ) {
        // HACK: A hack to work around not having a "solid earth" tile
        new_ter = t_dirt;
    }

    return new_ter;
}

// Check if there is supporting furniture cardinally adjacent to the bashed furniture
// For example, a washing machine behind the bashed door
static bool furn_is_supported( const map &m, const tripoint_bub_ms &p )
{
    const signed char cx[4] = { 0, -1, 0, 1};
    const signed char cy[4] = { -1,  0, 1, 0};

    for( int i = 0; i < 4; i++ ) {
        const point_bub_ms adj( p.xy() + point_rel_ms( cx[i], cy[i] ) );
        if( m.has_furn( tripoint_bub_ms( adj, p.z() ) ) &&
            m.furn( tripoint_bub_ms( adj, p.z() ) ).obj().has_flag( "BLOCKSDOOR" ) ) {
            return true;
        }
    }

    return false;
}

static auto get_sound_volume( const map_bash_info &bash, const bash_params &params ) -> int
{
    // Just take the minimum/base volume at 20dB.
    const auto minvol = 20;
    // Set maxvol to 140dB, which can be deafening for extreme impacts.
    const auto maxvol = 140;
    const auto impact_strength = params.destroy ? bash.str_max : params.strength;
    return bash.sound_vol.value_or( std::clamp( minvol + impact_strength, minvol, maxvol ) );
}

static void set_bash_sound_source( sound_event &se, const bash_params &params )
{
    if( !params.caused_by_player ) {
        return;
    }

    auto &player_character = get_avatar();
    se.from_player = true;
    se.faction = player_character.get_faction()->id;
    se.monfaction = player_character.get_faction()->mon_faction;
}

bash_results map::bash_ter_success( const tripoint_bub_ms &p, const bash_params &params )
{
    bash_results result;
    result.success = true;
    const ter_t &ter_before = ter( p ).obj();
    const map_bash_info &bash = ter_before.bash;
    if( has_flag_ter( "FUNGUS", p ) ) {
        fungal_effects( *g, *this ).create_spores( p );
    }
    const std::string soundfxvariant = ter_before.id.str();
    const bool will_collapse = ter_before.has_flag( TFLAG_SUPPORTS_ROOF );
    const bool suspended = ter_before.has_flag( TFLAG_SUSPENDED );
    bool follow_below = false;
    if( params.bashing_from_above && bash.ter_set_bashed_from_above ) {
        // If this terrain is being bashed from above and this terrain
        // has a valid post-destroy bashed-from-above terrain, set it
        ter_set( p, bash.ter_set_bashed_from_above );
    } else if( bash.ter_set ) {
        // If the terrain has a valid post-destroy terrain, set it
        ter_set( p, bash.ter_set );
        follow_below |= bash.bash_below;
    } else if( suspended ) {
        // Its important that we change the ter value before recursing, otherwise we'll hit an infinite loop.
        // This could be prevented by assembling a visited list, but in order to avoid that cost, we're going
        // build our recursion to just be resilient.
        ter_set( p, t_open_air );
        propagate_suspension_check( p );
    } else {
        tripoint_bub_ms below( p.xy(), p.z() - 1 );
        const ter_t &ter_below = ter( below ).obj();
        // Only setting the flag here because we want drops and sounds in correct order
        follow_below |= bash.bash_below && ter_below.roof;

        ter_set( p, t_open_air );
    }

    spawn_items( p, item_group::items_from( bash.drop_group, calendar::turn ) );

    if( !bash.sound.empty() && !params.silent ) {
        static const std::string soundfxid = "smash_success";
        const auto sound_volume = get_sound_volume( bash, params );
        sound_event se;
        se.origin = p;
        se.volume = sound_volume;
        se.category = sounds::sound_t::combat;
        se.description = bash.sound.translated();
        se.id = soundfxid;
        se.variant = soundfxvariant;
        set_bash_sound_source( se, params );
        sounds::sound( se );
    }

    if( follow_below || ter( p ) == t_open_air ) {
        const tripoint_bub_ms below( p.xy(), p.z() - 1 );
        // We may need multiple bashes in some weird cases
        // Example:
        //   W has roof A
        //   A bashes to B
        //   B bashes to nothing
        //   Below our point P, there is a W
        // If we bash down a B over a W, it might be from earlier A or just constructed over it!
        //
        // Current solution: bash roof until you reach same roof type twice, then bash down
        if( follow_below && params.do_recurse ) {
            bool blocked_by_roof = false;
            std::set<ter_id> encountered_types;
            encountered_types.insert( ter_before.id );
            encountered_types.insert( t_open_air );
            // Note: we're bashing the new roof, not the tile supported by it!
            int down_bash_tries = 10;
            do {
                const ter_id &ter_now = ter( p );
                if( encountered_types.contains( ter_now ) ) {
                    // We have encountered this type before and destroyed it (didn't block us)
                    ter_set( p, t_open_air );
                    bash_params params_below = params;
                    params_below.bashing_from_above = true;
                    params_below.bash_floor = false;
                    params_below.do_recurse = false;
                    params_below.destroy = true;
                    int impassable_bash_tries = 10;
                    // Unconditionally destroy, but don't go deeper
                    do {
                        result |= bash_ter_success( below, params_below );
                    } while( ter( below )->movecost == 0 && impassable_bash_tries-- > 0 );
                    if( impassable_bash_tries <= 0 ) {
                        debugmsg( "Loop in terrain bashing for type %s", ter_before.id.str() );
                    }
                } else if( ter_now == t_open_air ) {
                    const ter_id &roof = get_roof( below, params.bash_floor && ter( below )->movecost != 0 );
                    if( roof != t_open_air ) {
                        ter_set( p, roof );
                    }
                } else {
                    // This floor/roof tile wasn't destroyed in this loop yet
                    encountered_types.insert( ter_now );
                    bash_params params_copy = params;
                    params_copy.do_recurse = false;
                    // TODO: Unwrap the calls, don't recurse
                    // TODO: Don't bash furn
                    bash_results results_sub = bash_ter_furn( p, params_copy );
                    result |= results_sub;
                    if( !results_sub.success ) {
                        // Blocked, as in "the roof was too strong to bash"
                        blocked_by_roof = true;
                    }
                }
            } while( down_bash_tries-- > 0 && !blocked_by_roof &&
                     ( ter( p ) != t_open_air || ter( p )->movecost == 0 || ter( below )->roof ) );
            if( down_bash_tries <= 0 ) {
                debugmsg( "Loop in terrain bashing for type %s", ter_before.id.str() );
            }
        } else {
            const ter_id &roof = get_roof( below, params.bash_floor && ter( below )->movecost != 0 );

            ter_set( p, roof );
        }
    }

    if( will_collapse && !has_flag( TFLAG_SUPPORTS_ROOF, p ) ) {
        collapse_at( p, params.silent, true, bash.explosive > 0 );
    }

    if( bash.explosive > 0 ) {
        // TODO Implement if the player triggered the explosive terrain
        explosion_handler::explosion( p, nullptr, bash.explosive, 0.8, false );
    }

    return result;
}

bash_results map::bash_furn_success( const tripoint_bub_ms &p, const bash_params &params )
{
    bash_results result;
    const auto &furnid = furn( p ).obj();
    const map_bash_info &bash = furnid.bash;


    if( has_flag_furn( "FUNGUS", p ) ) {
        fungal_effects( *g, *this ).create_spores( p );
    }
    if( has_flag_furn( "MIGO_NERVE", p ) ) {
        map_funcs::migo_nerve_cage_removal( *this, p, true );
    }
    std::string soundfxvariant = furnid.id.str();
    const bool tent = !bash.tent_centers.empty();

    // Special code to collapse the tent if destroyed
    if( tent ) {
        // Get ids of possible centers
        std::set<furn_id> centers;
        for( const auto &cur_id : bash.tent_centers ) {
            if( cur_id.is_valid() ) {
                centers.insert( cur_id );
            }
        }

        std::optional<std::pair<tripoint_bub_ms, furn_id>> tentp;

        // Find the center of the tent
        // First check if we're not currently bashing the center
        if( centers.contains( furn( p ) ) ) {
            tentp.emplace( p, furn( p ) );
        } else {
            for( const tripoint_bub_ms &pt : points_in_radius( p, bash.collapse_radius ) ) {
                const furn_id &f_at = furn( pt );
                // Check if we found the center of the current tent
                if( centers.contains( f_at ) ) {
                    tentp.emplace( pt, f_at );
                    break;
                }
            }
        }
        // Didn't find any tent center, wreck the current tile
        if( !tentp ) {
            spawn_items( p, item_group::items_from( bash.drop_group, calendar::turn ) );
            release_avatar_grabbed_furniture_if_destroyed( p, furnid, bash.furn_set );
            furn_set( p, bash.furn_set );
        } else {
            // Take the tent down
            const int rad = tentp->second.obj().bash.collapse_radius;
            for( const auto &pt : points_in_radius( tripoint_bub_ms( tentp->first ), rad ) ) {
                const furn_id frn = furn( pt );
                if( frn == f_null ) {
                    continue;
                }

                const auto &furn_obj = frn.obj();
                const auto &recur_bash = furn_obj.bash;
                // Check if we share a center type and thus a "tent type"
                for( const auto &cur_id : recur_bash.tent_centers ) {
                    if( centers.contains( cur_id.id() ) ) {
                        // Found same center, wreck current tile
                        if( furn_obj.fluid_grid &&
                            furn_obj.fluid_grid->role == fluid_grid_role::tank ) {
                            fluid_grid::on_tank_removed( tripoint_abs_ms( map_local_to_abs( *this, pt ) ) );
                        }
                        spawn_items( p, item_group::items_from( recur_bash.drop_group, calendar::turn ) );
                        release_avatar_grabbed_furniture_if_destroyed( pt, furn_obj, recur_bash.furn_set );
                        furn_set( pt, recur_bash.furn_set );
                        break;
                    }
                }
            }
        }
        soundfxvariant = "smash_cloth";
    } else {
        if( furnid.fluid_grid && furnid.fluid_grid->role == fluid_grid_role::tank ) {
            fluid_grid::on_tank_removed( tripoint_abs_ms( map_local_to_abs( *this, p ) ) );
        }
        release_avatar_grabbed_furniture_if_destroyed( p, furnid, bash.furn_set );
        furn_set( p, bash.furn_set );
        for( item * const &it : i_at( p ) )  {
            it->on_drop( p, *this );
        }
        // HACK: Hack alert.
        // Signs have cosmetics associated with them on the submap since
        // furniture can't store dynamic data to disk. To prevent writing
        // mysteriously appearing for a sign later built here, remove the
        // writing from the submap.
        delete_signage( p );
    }

    if( !tent ) {
        spawn_items( p, item_group::items_from( bash.drop_group, calendar::turn ) );
    }

    if( !bash.sound.empty() && !params.silent ) {
        static const std::string soundfxid = "smash_success";
        const auto sound_volume = get_sound_volume( bash, params );
        sound_event se;
        se.origin = p;
        se.volume = sound_volume;
        se.category = sounds::sound_t::combat;
        se.description = bash.sound.translated();
        se.id = soundfxid;
        se.variant = soundfxvariant;
        set_bash_sound_source( se, params );
        sounds::sound( se );
    }

    if( bash.explosive > 0 ) {
        // TODO implement if the player triggered the explosive furniture
        explosion_handler::explosion( p, nullptr, bash.explosive, 0.8, false );
    }

    return result;
}

bash_results map::bash_ter_furn( const tripoint_bub_ms &p, const bash_params &params )
{
    bash_results result;
    std::string soundfxvariant;
    const auto &ter_obj = ter( p ).obj();
    const auto &furn_obj = furn( p ).obj();
    bool smash_ter = false;
    const map_bash_info *bash = nullptr;

    if( furn_obj.id && furn_obj.bash.str_max != -1 ) {
        bash = &furn_obj.bash;
        soundfxvariant = furn_obj.id.str();
    } else if( ter_obj.bash.str_max != -1 ) {
        bash = &ter_obj.bash;
        smash_ter = true;
        soundfxvariant = ter_obj.id.str();
    }

    // Floor bashing check
    // Only allow bashing floors when we want to bash floors and we're in z-level mode
    // Unless we're destroying, then it gets a little weird
    if( smash_ter && bash->bash_below && !params.bash_floor ) {
        if( !params.destroy ) {
            smash_ter = false;
            bash = nullptr;
        } else if( !bash->ter_set ) {
            // HACK: A hack for destroy && !bash_floor
            // We have to check what would we create and cancel if it is what we have now
            tripoint_bub_ms below( p.xy(), p.z() - 1 );
            const auto roof = get_roof( below, false );
            if( roof == ter( p ) ) {
                smash_ter = false;
                bash = nullptr;
            }
        } else if( !bash->ter_set && ter( p ) == t_dirt ) {
            // As above, except for no-z-levels case
            smash_ter = false;
            bash = nullptr;
        }
    }

    // TODO: what if silent is true?
    if( has_flag( "ALARMED", p ) && !g->timed_events.queued( TIMED_EVENT_WANTED ) ) {
        sound_event se;
        se.origin = p;
        se.volume = 90;
        se.category = sounds::sound_t::alarm;
        se.description = _( "an alarm go off!" );
        se.id = "environment";
        se.variant = "alarm";
        sounds::sound( se );
        // Blame nearby player
        if( rl_dist( g->u.bub_pos(), p ) <= 3 ) {
            g->events().send<event_type::triggers_alarm>( g->u.getID() );
            const auto abs = project_to<coords::sm>( map_local_to_abs( *this, p.xy() ) );
            g->timed_events.add( TIMED_EVENT_WANTED, calendar::turn + 30_minutes, 0,
                                 tripoint_abs_sm( abs, p.z() ) );
        }
    }

    if( bash == nullptr || ( bash->destroy_only && !params.destroy ) ) {
        // Nothing bashable here
        if( impassable( p ) ) {
            if( !params.silent ) {
                sound_event se;
                se.origin = p;
                se.volume = 80;
                se.category = sounds::sound_t::combat;
                se.description = _( "thump!" );
                se.id = "smash_fail";
                se.variant = "default";
                set_bash_sound_source( se, params );
                sounds::sound( se );
            }

            result.did_bash = true;
            result.bashed_solid = true;
        }

        return result;
    }

    result.did_bash = true;
    result.bashed_solid = true;
    result.success = params.destroy;

    int smin = bash->str_min;
    int smax = bash->str_max;
    if( !params.destroy ) {
        if( bash->str_min_blocked != -1 || bash->str_max_blocked != -1 ) {
            if( furn_is_supported( *this, p ) ) {
                if( bash->str_min_blocked != -1 ) {
                    smin = bash->str_min_blocked;
                }
                if( bash->str_max_blocked != -1 ) {
                    smax = bash->str_max_blocked;
                }
            }
        }

        if( bash->str_min_supported != -1 || bash->str_max_supported != -1 ) {
            tripoint_bub_ms below( p.xy(), p.z() - 1 );
            if( has_flag( TFLAG_SUPPORTS_ROOF, below ) ) {
                if( bash->str_min_supported != -1 ) {
                    smin = bash->str_min_supported;
                }
                if( bash->str_max_supported != -1 ) {
                    smax = bash->str_max_supported;
                }
            }
        }
        // Linear interpolation from str_min to str_max
        const int resistance = smin + ( params.roll * ( smax - smin ) );
        if( params.strength >= resistance ) {
            result.success = true;
        }
    }

    if( !result.success ) {
        // Cap out bash volume to 120dB for sanity checking.
        int sound_volume = std::min( 120, bash->sound_fail_vol.value_or( 70 ) );

        result.did_bash = true;
        if( !params.silent ) {
            sound_event se;
            se.origin = p;
            se.volume = sound_volume;
            se.category = sounds::sound_t::combat;
            se.description = bash->sound_fail.translated();
            se.id = "smash_fail";
            se.variant = soundfxvariant;
            set_bash_sound_source( se, params );
            sounds::sound( se );
        }

        if( !smash_ter && smax > 0 ) {
            const auto flipped_version = get_furn_transforms_into( p );
            if( flipped_version != furn_str_id::NULL_ID() ) {
                const int damage_percent = ( params.strength * 100 ) / smax;
                if( rng( 1, 100 ) <= damage_percent ) {
                    furn_set( p, flipped_version );
                }
            }
        }
        // Hard impacts have a chance to dislodge targets perching above
        if( params.strength >= smin / 2 && one_in( smin / 2 ) ) {
            tripoint_bub_ms above( p.xy(), p.z() + 1 );
            Character *character = g->critter_at<Character>( above );
            if( has_flag( TFLAG_UNSTABLE, above ) && character != nullptr ) {
                character->add_msg_if_player( m_warning,
                                              _( "You feel the ground beneath you shake from the impact!" ) );

                if( character->stability_roll() < rng( 1, params.strength - ( smin / 2 ) ) ) {
                    character->add_msg_player_or_npc( m_bad, _( "You lose your balance!" ),
                                                      _( "<npcname> loses their balance!" ) );

                    g->fling_creature( character, rng_float( 0_degrees, 360_degrees ), 10 );
                }

            }
        }
    } else {
        if( smash_ter ) {
            result |= bash_ter_success( p, params );
        } else {
            result |= bash_furn_success( p, params );
        }
    }

    return result;
}

bash_results map::bash( const tripoint_bub_ms &p, const int str,
                        bool silent, bool destroy, bool bash_floor,
                        const vehicle *bashing_vehicle )
{
    const auto bsh = bash_params{
        .strength = str,
        .silent = silent,
        .destroy = destroy,
        .bash_floor = bash_floor,
        .roll = static_cast<float>( rng_float( 0, 1.0f ) ),
        .bashing_from_above = false,
        .do_recurse = true
    };
    return bash( p, bsh, bashing_vehicle );
}

bash_results map::bash( const tripoint_bub_ms &p, const bash_params &bsh,
                        const vehicle *bashing_vehicle )
{
    bash_results result;

    // Dimension bounds cannot be bashed - show message from boundary terrain
    if( get_mapbuffer().is_outside_pocket_dimension_bounds( map_local_to_abs( *this, p ) ) ) {
        const auto &pocket_info = get_mapbuffer().get_pocket_info();
        if( !bsh.silent && pocket_info ) {
            const ter_t &boundary_ter = pocket_info->bounds.boundary_terrain.obj();
            if( !boundary_ter.bash.sound_fail.empty() ) {
                add_msg( m_info, boundary_ter.bash.sound_fail.translated() );
            }
        }
        return result;  // Cannot bash dimension boundary
    }

    bool bashed_sealed = false;
    if( has_flag( "SEALED", p ) ) {
        result |= bash_ter_furn( p, bsh );
        bashed_sealed = true;
    }

    result |= bash_field( p, bsh );

    // Don't bash items inside terrain/furniture with SEALED flag
    if( !bashed_sealed ) {
        result |= bash_items( p, bsh );
    }
    // Don't bash the vehicle doing the bashing
    const vehicle *veh = veh_pointer_or_null( veh_at( p ) );
    if( veh != nullptr && veh != bashing_vehicle ) {
        result |= bash_vehicle( p, bsh );
    }

    // If we still didn't bash anything solid (a vehicle) or a tile with SEALED flag, bash ter/furn
    if( !result.bashed_solid && !bashed_sealed ) {
        result |= bash_ter_furn( p, bsh );
    }

    return result;
}

bash_results map::bash_items( const tripoint_bub_ms &p, const bash_params &params )
{
    bash_results result;
    if( !has_items( p ) ) {
        return result;
    }

    std::vector<detached_ptr<item>> smashed_contents;
    auto bashed_items = i_at( p );
    bool smashed_glass = false;
    for( auto bashed_item = bashed_items.begin(); bashed_item != bashed_items.end(); ) {
        // the check for active suppresses Molotovs smashing themselves with their own explosion
        if( ( *bashed_item )->can_shatter() && !( *bashed_item )->is_active() &&
            one_in( 2 ) ) {
            result.did_bash = true;
            smashed_glass = true;
            for( detached_ptr<item> &bashed_content : ( *bashed_item )->contents.clear_items() ) {
                smashed_contents.push_back( std::move( bashed_content ) );
            }
            bashed_item = bashed_items.erase( bashed_item );
        } else {
            ++bashed_item;
        }
    }
    // Now plunk in the contents of the smashed items.
    spawn_items( p, std::move( smashed_contents ) );

    // Add a glass sound even when something else also breaks
    if( smashed_glass && !params.silent ) {
        sound_event se;
        se.origin = p;
        se.volume = 70;
        se.category = sounds::sound_t::combat;
        se.description = _( "glass shattering" );
        se.id = "smash_success";
        se.variant = "smash_glass_contents";
        set_bash_sound_source( se, params );
        sounds::sound( se );
    }
    return result;
}

bash_results map::bash_vehicle( const tripoint_bub_ms &p, const bash_params &params )
{
    bash_results result;
    // Smash vehicle if present
    if( const optional_vpart_position vp = veh_at( p ) ) {
        vp->vehicle().damage( vp->part_index(), params.strength, DT_BASH, true );
        if( !params.silent ) {
            sound_event se;
            se.origin = p;
            se.volume = 70;
            se.category = sounds::sound_t::combat;
            se.description = _( "crash!" );
            se.id = "smash_success";
            se.variant = "hit_vehicle";
            set_bash_sound_source( se, params );
            sounds::sound( se );
        }

        result.did_bash = true;
        result.success = true;
        result.bashed_solid = true;
    }
    return result;
}

bash_results map::bash_field( const tripoint_bub_ms &p, const bash_params & )
{
    bash_results result;
    if( get_field( p, fd_web ) != nullptr ) {
        result.did_bash = true;
        result.bashed_solid = true; // To prevent bashing furniture/vehicles
        remove_field( p, fd_web );
    }

    return result;
}

bash_results &bash_results::operator|=( const bash_results &other )
{
    did_bash |= other.did_bash;
    success |= other.success;
    bashed_solid |= other.bashed_solid;
    subresults.push_back( other );
    return *this;
}

void map::destroy( const tripoint_bub_ms &p, const bool silent )
{
    // Dimension bounds cannot be destroyed
    if( get_mapbuffer().is_outside_pocket_dimension_bounds( map_local_to_abs( *this, p ) ) ) {
        return;
    }

    // Break if it takes more than 25 destructions to remove to prevent infinite loops
    // Example: A bashes to B, B bashes to A leads to A->B->A->...

    // If we were destroying a floor, allow destroying floors
    // If we were destroying something unpassable, destroy only that
    bool was_impassable = impassable( p );
    int count = 0;
    while( count <= 25
           && bash( p, 999, silent, true ).success
           && ( !was_impassable || impassable( p ) ) ) {
        count++;
    }
}

void map::destroy_furn( const tripoint_bub_ms &p, const bool silent )
{
    // Break if it takes more than 25 destructions to remove to prevent infinite loops
    // Example: A bashes to B, B bashes to A leads to A->B->A->...
    int count = 0;
    while( count <= 25 && furn( p ) != f_null && bash( p, 999, silent, true ).success ) {
        count++;
    }
}

void map::batter( const tripoint_bub_ms &p, int power, int tries, const bool silent )
{
    int count = 0;
    while( count < tries && bash( p, power, silent ).success ) {
        count++;
    }
}

void map::crush( const tripoint_bub_ms &p )
{
    player *crushed_player = g->critter_at<player>( p );

    if( crushed_player != nullptr ) {
        bool player_inside = false;
        if( crushed_player->in_vehicle ) {
            const optional_vpart_position vp = veh_at( p );
            player_inside = vp && vp->is_inside();
        }
        if( !player_inside ) { //If there's a player at p and he's not in a covered vehicle...
            //This is the roof coming down on top of us, no chance to dodge
            crushed_player->add_msg_player_or_npc( m_bad, _( "You are crushed by the falling debris!" ),
                                                   _( "<npcname> is crushed by the falling debris!" ) );
            // TODO: Make this depend on the ceiling material
            const int dam = rng( 0, 40 );
            // Torso and head take the brunt of the blow
            crushed_player->deal_damage( nullptr, bodypart_id( "head" ), damage_instance( DT_BASH,
                                         dam * .25 ) );
            crushed_player->deal_damage( nullptr, bodypart_id( "torso" ), damage_instance( DT_BASH,
                                         dam * .45 ) );
            // Legs take the next most through transferred force
            crushed_player->deal_damage( nullptr, bodypart_id( "leg_l" ), damage_instance( DT_BASH,
                                         dam * .10 ) );
            crushed_player->deal_damage( nullptr, bodypart_id( "leg_r" ), damage_instance( DT_BASH,
                                         dam * .10 ) );
            // Arms take the least
            crushed_player->deal_damage( nullptr, bodypart_id( "arm_l" ), damage_instance( DT_BASH,
                                         dam * .05 ) );
            crushed_player->deal_damage( nullptr, bodypart_id( "arm_r" ), damage_instance( DT_BASH,
                                         dam * .05 ) );

            // Pin whoever got hit
            crushed_player->add_effect( effect_crushed, 1_turns, bodypart_str_id::NULL_ID() );
            crushed_player->check_dead_state();
        }
    }

    if( monster *const monhit = g->critter_at<monster>( p ) ) {
        // 25 ~= 60 * .45 (torso)
        monhit->deal_damage( nullptr, bodypart_id( "torso" ), damage_instance( DT_BASH, rng( 0, 25 ) ) );

        // Pin whoever got hit
        monhit->add_effect( effect_crushed, 1_turns, bodypart_str_id::NULL_ID() );
        monhit->check_dead_state();
    }

    if( const optional_vpart_position vp = veh_at( p ) ) {
        // Arbitrary number is better than collapsing house roof crushing APCs
        vp->vehicle().damage( vp->part_index(), rng( 100, 1000 ), DT_BASH, false );
    }
}

void map::shoot( const tripoint_bub_ms &origin, const tripoint_bub_ms &p, projectile &proj,
                 const bool hit_items )
{
    float initial_damage = 0.0;
    float initial_arpen = 0.0;
    float initial_armor_mult = 1.0;
    for( const damage_unit &dam : proj.impact ) {
        initial_damage += dam.amount * dam.damage_multiplier;
        initial_arpen += dam.res_pen;
        initial_armor_mult *= dam.res_mult;
    }
    if( initial_damage < 0 ) {
        return;
    }

    float dam = initial_damage;
    float pen = initial_arpen;

    if( has_flag( "ALARMED", p ) && !g->timed_events.queued( TIMED_EVENT_WANTED ) ) {
        sound_event se;
        se.origin = p;
        se.volume = 90;
        se.category = sounds::sound_t::alarm;
        se.description = _( "an alarm sound!" );
        se.id = "environment";
        se.variant = "alarm";
        sounds::sound( se );
        const auto abs = project_to<coords::sm>( bub_to_abs( p ) );
        g->timed_events.add( TIMED_EVENT_WANTED, calendar::turn + 30_minutes, 0, abs );
    }

    const bool inc = proj.has_effect( ammo_effect_INCENDIARY ) ||
                     proj.impact.type_damage( DT_HEAT ) > 0;
    const bool phys = proj.impact.type_damage( DT_BASH ) > 0 ||
                      proj.impact.type_damage( DT_CUT ) > 0 ||
                      proj.impact.type_damage( DT_STAB ) > 0 ||
                      proj.impact.type_damage( DT_BULLET ) > 0;
    if( const optional_vpart_position vp = veh_at( p ) ) {
        if( origin.z() > p.z() && vp->part_with_feature( "ROOF", true ) ) {
            const int roof = vp->vehicle().roof_at_part( vp->part_index() );
            dam = vp->vehicle().damage( roof, dam, inc ? DT_HEAT : DT_STAB, hit_items, false );
        } else {
            dam = vp->vehicle().damage( vp->part_index(), dam, inc ? DT_HEAT : DT_STAB, hit_items );
        }
    }

    furn_id furn_here = furn( p );
    furn_t furn = furn_here.obj();

    ter_id terrain = ter( p );
    ter_t ter = terrain.obj();

    double range = rl_dist( origin, p );
    const bool point_blank = range <= 1;
    if( furn.bash.ranged ) {
        // Damage cover like a crit if we're breaching at point blank range, otherwise randomize like a normal hit.
        float destroy_roll = point_blank ? dam * 1.5 : dam * rng_float( 0.9, 1.1 );
        const ranged_bash_info &rfi = *furn.bash.ranged;
        if( !hit_items && ( !check( rfi.block_unaimed_chance ) || ( rfi.block_unaimed_chance < 100_pct &&
                            point_blank ) ) ) {
            // Nothing, it's a miss, we're shooting over nearby furniture.
        } else if( proj.has_effect( ammo_effect_NO_PENETRATE_OBSTACLES ) ) {
            // We shot something with a flamethrower or other non-penetrating weapon.
            // Try to bash the obstacle and stop the shot.
            if( get_avatar().sees( p ) ) {
                add_msg( _( "The shot strikes the %s!" ), furnname( p ) );
            }
            if( phys ) {
                bash( p, dam, false );
            }
            dam = 0;
        } else if( rfi.reduction_laser && proj.has_effect( ammo_effect_LASER ) ) {
            dam -= std::max( ( rng( rfi.reduction_laser->min,
                                    rfi.reduction_laser->max ) - pen ) * initial_armor_mult, 0.0f );
        } else {
            // Roll damage reduction value, reduce result by arpen, multiply by any armor mult, then finally set to zero if negative result
            const float pen_reduction = rng( rfi.reduction.min, rfi.reduction.max );
            dam = std::max( dam - ( std::max( pen_reduction - pen, 0.0f ) * initial_armor_mult ),
                            0.0f );
            pen = std::max( 0.0f, pen - pen_reduction );
            // Only print if we hit something we can see enemies through, so we know cover did its job
            if( get_avatar().sees( p ) ) {
                if( dam <= 0 ) {
                    add_msg( _( "The shot is stopped by the %s!" ), furnname( p ) );
                    // Only bother mentioning it punched through if it had any resistance, so zip through canvas with no message.
                } else if( rfi.reduction.min > 0 ) {
                    add_msg( _( "The shot hits the %s and punches through!" ), furnname( p ) );
                }
            }
            add_msg( m_debug, "%s: damage: %.0f -> %.0f, arpen: %.0f -> %.0f", furn.name(), initial_damage, dam,
                     initial_arpen,
                     pen );
            if( destroy_roll > rfi.destroy_threshold && rfi.reduction.min > 0 ) {
                bash_params params{0, false, true, hit_items, 1.0, false};
                bash_furn_success( p, params );
            }
            if( rfi.flammable && inc ) {
                add_field( p, fd_fire, 1 );
            }
        }
        // Check furniture and terrain separately, if this was an if/else then getting partial cover embedded in a wall would let you fire through it.
    }
    if( ter.bash.ranged ) {
        // New values are used for debug message in case furniture did something.
        float modified_dam = dam;
        float modified_pen = pen;
        // Separate hit roll since damage might have been lowered by furniture first.
        float destroy_roll = point_blank ? dam * 1.5 : dam * rng_float( 0.9, 1.1 );
        const ranged_bash_info &ri = *ter.bash.ranged;
        if( !hit_items && ( !check( ri.block_unaimed_chance ) || ( ri.block_unaimed_chance < 100_pct &&
                            point_blank ) ) ) {
            // Nothing, it's a miss or we're shooting over nearby terrain
        } else if( proj.has_effect( ammo_effect_NO_PENETRATE_OBSTACLES ) ) {
            // We shot something with a flamethrower or other non-penetrating weapon.
            // Try to bash the obstacle if it was a thrown rock or the like, then stop the shot.
            if( get_avatar().sees( p ) ) {
                add_msg( _( "The shot strikes the %s!" ), tername( p ) );
            }
            if( phys ) {
                bash( p, dam, false );
            }
            dam = 0;
        } else if( ri.reduction_laser && proj.has_effect( ammo_effect_LASER ) ) {
            dam -= std::max( ( rng( ri.reduction_laser->min,
                                    ri.reduction_laser->max ) - pen ) * initial_armor_mult, 0.0f );
        } else {
            // Roll damage reduction value, reduce result by arpen, multiply by any armor mult, then finally set to zero if negative result
            const float pen_reduction = rng( ri.reduction.min, ri.reduction.max );
            dam = std::max( dam - ( std::max( pen_reduction - pen, 0.0f ) * initial_armor_mult ),
                            0.0f );
            pen = std::max( 0.0f, pen - pen_reduction );
            // Only print if we hit something we can see enemies through, so we know cover did its job
            if( get_avatar().sees( p ) ) {
                if( dam <= 0 ) {
                    add_msg( _( "The shot is stopped by the %s!" ), tername( p ) );
                    // Only bother mentioning it punched through if it had any resistance, so zip through canvas with no message.
                } else if( ri.reduction.min > 0 ) {
                    add_msg( _( "The shot hits the %s and punches through!" ), tername( p ) );
                }
            }
            add_msg( m_debug, "%s: damage: %.0f -> %.0f, arpen: %.0f -> %.0f", ter.name(), modified_dam, dam,
                     modified_pen,
                     pen );
            // Destroy if the damage exceeds threshold, unless the target was meant to be shot through with zero resistance like canvas.
            if( destroy_roll > ri.destroy_threshold && ri.reduction.min > 0 ) {
                bash_params params{0, false, true, hit_items, 1.0, false};
                bash_ter_success( p, params );
            }
            if( ri.flammable && inc ) {
                add_field( p, fd_fire, 1 );
            }
        }
    } else if( impassable( p ) && !is_transparent( p ) ) {
        bash( p, dam, false );
        // TODO: Preserve some residual damage when it makes sense.
        dam = 0;
    }

    apply_ammo_trail_effects( p, proj.get_ammo_effects(), 1.0 );

    // Check fields?
    const field_entry *fieldhit = get_field( p, fd_web );
    if( fieldhit != nullptr ) {
        if( inc ) {
            add_field( p, fd_fire, fieldhit->get_field_intensity() - 1 );
        } else if( dam > 5 + fieldhit->get_field_intensity() * 5 &&
                   one_in( 5 - fieldhit->get_field_intensity() ) ) {
            dam -= rng( 1, 2 + ( fieldhit->get_field_intensity() * 2 ) );
            remove_field( p, fd_web );
        }
    }

    // Rescale the damage
    if( dam <= 0 ) {
        proj.impact.damage_units.clear();
        return;
    } else if( dam < initial_damage ) {
        proj.impact.mult_damage( dam / static_cast<double>( initial_damage ) );
    }
    if( pen <= 0 ) {
        for( auto &elem : proj.impact.damage_units ) {
            elem.res_pen = 0.0f;
        }
    } else if( pen < initial_arpen ) {
        for( auto &elem : proj.impact.damage_units ) {
            elem.res_pen *= ( pen / static_cast<double>( initial_arpen ) );
        }
    }

    //Projectiles with NO_ITEM_DAMAGE flag won't damage items at all
    if( !hit_items ) {
        return;
    }

    // Make sure the message is sensible for the ammo effects. Lasers aren't projectiles.
    std::string damage_message;
    if( proj.has_effect( ammo_effect_LASER ) ) {
        damage_message = _( "laser beam" );
    } else if( proj.has_effect( ammo_effect_LIGHTNING ) ) {
        damage_message = _( "bolt of electricity" );
    } else if( proj.has_effect( ammo_effect_PLASMA ) ) {
        damage_message = _( "bolt of plasma" );
    } else {
        damage_message = _( "flying projectile" );
    }

    smash_trap( p, dam, string_format( _( "The %1$s" ), damage_message ) );
    smash_items( p, dam, damage_message, false );
}

bool map::hit_with_acid( const tripoint_bub_ms &p )
{
    if( passable( p ) ) {
        return false;    // Didn't hit the tile!
    }
    const ter_id t = ter( p );
    if( t == t_wall_glass || t == t_wall_glass_alarm ||
        t == t_vat ) {
        ter_set( p, t_floor );
    } else if( t == t_door_c || t == t_door_locked || t == t_door_locked_peep ||
               t == t_door_locked_alarm ) {
        if( one_in( 3 ) ) {
            ter_set( p, t_door_b );
        }
    } else if( t == t_door_bar_c || t == t_door_bar_o || t == t_door_bar_locked || t == t_bars ||
               t == t_reb_cage ) {
        ter_set( p, t_floor );
        add_msg( m_warning, _( "The metal bars melt!" ) );
    } else if( t == t_door_b ) {
        if( one_in( 4 ) ) {
            ter_set( p, t_door_frame );
        } else {
            return false;
        }
    } else if( t == t_window || t == t_window_alarm || t == t_window_no_curtains ) {
        ter_set( p, t_window_empty );
    } else if( t == t_wax ) {
        ter_set( p, t_floor_wax );
    } else if( t == t_gas_pump || t == t_gas_pump_smashed ) {
        return false;
    } else if( t == t_card_science || t == t_card_military || t == t_card_industrial ) {
        ter_set( p, t_card_reader_broken );
    }
    return true;
}

// returns true if terrain stops fire
bool map::hit_with_fire( const tripoint_bub_ms &p )
{
    if( passable( p ) ) {
        return false;    // Didn't hit the tile!
    }

    // non passable but flammable terrain, set it on fire
    if( has_flag( "FLAMMABLE", p ) || has_flag( "FLAMMABLE_ASH", p ) ) {
        add_field( p, fd_fire, 3 );
    }
    return true;
}

bool map::can_open_door(
    const const_interacting_entity &who, const tripoint_bub_ms &p, bool inside
) const
{

    const auto &ter = this->ter( p ).obj();
    if( ter.open ) {
        return can_open_door_ter( who, ter, p, inside );
    }

    const auto &furn = this->furn( p ).obj();
    if( furn.open ) {
        return can_open_door_furn( who, furn, p, inside );
    }

    const optional_vpart_position vp = veh_at( p );
    if( vp ) {
        return can_open_door_veh( who, vp, p, inside );
    }

    return false;

}


bool map::open_door(
    const interacting_entity &who,  const tripoint_bub_ms &p, const bool inside
)
{
    const auto &ter = this->ter( p ).obj();
    if( ter.open ) {
        return open_door_ter( who, ter, p, inside );
    }

    const auto &furn = this->furn( p ).obj();
    if( furn.open ) {
        return open_door_furn( who, furn, p, inside );
    }

    const optional_vpart_position vp = veh_at( p );
    if( vp ) {
        return open_door_veh( who, vp, p, inside );
    }
    return false;
}

struct can_open_while_mounted {
    template<typename T>
    auto operator()( T u ) -> bool {
        constexpr auto is_const_char = std::is_same_v<Character *, T>;
        constexpr auto is_char = std::is_same_v<const Character *, T>;
        if constexpr( is_const_char || is_char ) {
            if( u->is_mounted() ) {
                auto mon = u->mounted_creature.get();
                if( !mon->has_flag( MF_MOUNTABLE_DOORS ) ) {
                    u->add_msg_if_player( m_info, _( "You can't open things while you're riding." ) );
                    return false;
                }
            }
        }
        return true;
    };
};

bool map::can_open_door_ter(
    const const_interacting_entity &who, const ter_t &,
    const tripoint_bub_ms &p, bool inside
) const
{

    if( has_flag( str_OPENCLOSE_INSIDE, p ) && !inside ) {
        return false;
    }

    if( !std::visit( can_open_while_mounted{}, who ) ) {
        return false;
    }

    return true;
}


bool map::open_door_ter(
    const interacting_entity &who, const ter_t &ter,
    const tripoint_bub_ms &p, const bool inside
)
{
    if( !can_open_door_ter( static_variant_cast<const_interacting_entity>( who ), ter, p, inside ) ) {
        return false;
    }

    sound_event se;
    se.origin = p;
    se.volume = 50;
    se.category = sounds::sound_t::movement;
    se.movement_noise = true;
    se.description = _( "swish" );
    se.id = "open_door";
    se.variant = ter.id.str();
    sounds::sound( se );
    ter_set( p, ter.open );

    const auto is_schizo = std::visit( []<typename T>( T u ) -> bool {
        if constexpr( std::is_same_v<T, Character *> )
        {
            return u->has_trait( trait_id( "SCHIZOPHRENIC" ) ) || u->has_artifact_with( AEP_SCHIZO );
        }
        return false;
    }, who );

    const tripoint_bub_ms you_pos = std::visit( []<typename T>( T u ) { return u->bub_pos(); }, who );

    if( is_schizo
        && one_in( 50 )
        && !ter.has_flag( "TRANSPARENT" ) ) {
        // This math is schizophrenic
        const tripoint_bub_ms mp =
            p + -2 * you_pos.xy().raw() + tripoint_rel_ms( 2 * p.x(), 2 * p.y(), p.z() );
        g->spawn_hallucination( mp );
    }

    return true;

}

bool map::can_open_door_furn(
    const const_interacting_entity &who, const furn_t &,
    const tripoint_bub_ms &p, bool inside
) const
{

    if( has_flag( str_OPENCLOSE_INSIDE, p ) && !inside ) {
        return false;
    }

    if( !std::visit( can_open_while_mounted{}, who ) ) {
        return false;
    }

    return true;
}


bool map::open_door_furn(
    const interacting_entity &who, const furn_t &furn,
    const tripoint_bub_ms &p, const bool inside
)
{
    if( !can_open_door_furn( static_variant_cast<const_interacting_entity>( who ), furn, p, inside ) ) {
        return false;
    }

    sound_event se;
    se.origin = p;
    se.volume = 50;
    se.category = sounds::sound_t::movement;
    se.movement_noise = true;
    se.description = _( "swish" );
    se.id = "open_door";
    se.variant = furn.id.str();
    sounds::sound( se );
    furn_set( p, furn.open );
    return true;

}

bool map::can_open_door_veh(
    const const_interacting_entity &who, const optional_vpart_position &vp,
    const tripoint_bub_ms &, bool inside
) const
{

    const int openable = vp->vehicle().next_part_to_open( vp->part_index(), !inside );
    if( openable < 0 ) {
        const int openable_other_way = vp->vehicle().next_part_to_open( vp->part_index(), !inside );
        if( openable_other_way >= 0 ) {
            const auto you = std::visit( []( auto &&v ) { return static_cast<const Creature *>( v );}, who );
            if( inside ) {
                you->add_msg_if_player( _( "The %1$s's %2$s can only be opened from outside." ), vp->vehicle().name,
                                        vp->vehicle().part_info( vp->part_index() ).name() );
            } else {
                you->add_msg_if_player( _( "The %1$s's %2$s can only be opened from inside." ), vp->vehicle().name,
                                        vp->vehicle().part_info( vp->part_index() ).name() );
            }
        }
        return false;
    }

    if( !std::visit( can_open_while_mounted{}, who ) ) {
        return false;
    }

    return true;
}


bool map::open_door_veh(
    const interacting_entity &who, const optional_vpart_position &vp,
    const tripoint_bub_ms &p, bool inside
)
{
    if( !can_open_door_veh( static_variant_cast<const_interacting_entity>( who ), vp, p, inside ) ) {
        return false;
    }

    if( std::holds_alternative<Character *>( who ) ) {
        auto &you = *std::get<Character *>( who );
        if( you.is_avatar() &&
            !vp->vehicle().handle_potential_theft( *you.as_avatar() ) ) {
            return false;
        }
    }

    const auto is_owner = std::visit(
    [&]<typename T>( T u ) -> bool {
        if constexpr( std::is_same_v<T, Character *> )
        {
            return vp->vehicle().is_owned_by( *u );
        }
        return false;
    },
    who );

    const auto lock_part = vp.part_with_feature( str_DOOR_LOCKING, true );
    const bool has_locked_door = lock_part.has_value() && lock_part.value().part().enabled;
    // vehicle::is_locked = you have no keys / vehicle has not been hotwired yet
    // unrelated to the door lock itself
    if( has_locked_door && ( !is_owner || vp->vehicle().is_locked ) ) {

        const auto &veh = vp->vehicle();
        const auto you = std::visit( []( auto &&v ) { return static_cast<const Creature *>( v );}, who );
        const auto dpart = veh.next_part_to_open( vp->part_index(), !inside );
        you->add_msg_if_player( _( "The %1$s's %2$s is locked." ), veh.name,
                                veh.part_info( dpart ).name() );

        return false;
    }

    const int openable = vp->vehicle().next_part_to_open( vp->part_index(), !inside );
    vp->vehicle().open_all_at( openable );
    return true;
}

void map::translate( const ter_id &from, const ter_id &to )
{
    if( from == to ) {
        debugmsg( "map::translate %s => %s",
                  from.obj().name(),
                  from.obj().name() );
        return;
    }
    for( const tripoint_bub_ms &p : points_on_zlevel() ) {
        if( ter( p ) == from ) {
            ter_set( p, to );
        }
    }
}

//This function performs the translate function within a given radius of the player.
void map::translate_radius( const ter_id &from, const ter_id &to, float radi,
                            const tripoint_bub_ms &p,
                            const bool same_submap, const bool toggle_between )
{
    if( from == to ) {
        debugmsg( "map::translate %s => %s", from.obj().name(), to.obj().name() );
        return;
    }

    const tripoint_abs_omt abs_omt_p = project_to<coords::omt>( map_local_to_abs( *this, p ) );
    for( const tripoint_bub_ms &t : points_on_zlevel() ) {
        const tripoint_abs_omt abs_omt_t = project_to<coords::omt>( map_local_to_abs( *this, t ) );
        const float radiX = trig_dist( p, t );
        if( ter( t ) == from ) {
            // within distance, and either no submap limitation or same overmap coords.
            if( radiX <= radi && ( !same_submap || abs_omt_t == abs_omt_p ) ) {
                ter_set( t, to );
            }
        } else if( toggle_between && ter( t ) == to ) {
            if( radiX <= radi && ( !same_submap || abs_omt_t == abs_omt_p ) ) {
                ter_set( t, from );
            }
        }
    }
}

bool map::close_door( const tripoint_bub_ms &p, const bool inside, const bool check_only )
{
    if( has_flag( str_OPENCLOSE_INSIDE, p ) && !inside ) {
        return false;
    }

    const auto &ter = this->ter( p ).obj();
    const auto &furn = this->furn( p ).obj();
    sound_event se;
    se.origin = p;
    se.volume = 60;
    se.category = sounds::sound_t::movement;
    se.movement_noise = true;
    se.description = _( "swish" );
    se.id = "close_door";
    se.variant = ter.id.str();
    if( ter.close && !furn.id ) {
        if( !check_only ) {
            sounds::sound( se );
            ter_set( p, ter.close );
        }
        return true;
    } else if( furn.close ) {
        if( !check_only ) {
            sounds::sound( se );
            furn_set( p, furn.close );
        }
        return true;
    }
    return false;
}

std::string map::get_signage( const tripoint_bub_ms &p ) const
{
    return get_mapbuffer().get_signage( map_local_to_abs( *this, p ),
                                        resident_item_lookup() ).value_or( "" );
}
void map::set_signage( const tripoint_bub_ms &p, const std::string &message ) const
{
    get_mapbuffer().set_signage( map_local_to_abs( *this, p ), message, resident_item_lookup() );
}
void map::delete_signage( const tripoint_bub_ms &p ) const
{
    get_mapbuffer().delete_signage( map_local_to_abs( *this, p ), resident_item_lookup() );
}

int map::get_radiation( const tripoint_bub_ms &p ) const
{
    return get_mapbuffer().get_radiation( map_local_to_abs( *this, p ),
                                          resident_item_lookup() ).value_or( 0 );
}

void map::set_radiation( const tripoint_bub_ms &p, const int value )
{
    get_mapbuffer().set_radiation( map_local_to_abs( *this, p ), value, resident_item_lookup() );
}

void map::adjust_radiation( const tripoint_bub_ms &p, const int delta )
{
    get_mapbuffer().adjust_radiation( map_local_to_abs( *this, p ), delta, resident_item_lookup() );
}

int map::get_temperature( const tripoint_bub_ms &p ) const
{
    const auto abs_pos = map_local_to_abs( *this, p );
    if( get_mapbuffer().is_outside_pocket_dimension_bounds( abs_pos ) ) {
        return 0;
    }

    return get_mapbuffer().get_temperature( abs_pos, resident_item_lookup() ).value_or( 0 );
}

void map::set_temperature( const tripoint_bub_ms &p, int new_temperature )
{
    const auto abs_pos = map_local_to_abs( *this, p );
    if( get_mapbuffer().is_outside_pocket_dimension_bounds( abs_pos ) ) {
        return;
    }

    get_mapbuffer().set_temperature( abs_pos, new_temperature, resident_item_lookup() );
}
// Items: 3D

map_stack map::i_at( const tripoint_bub_ms &p )
{
    const auto abs_pos = map_local_to_abs( *this, p );
    point_sm_ms l;
    submap *const current_submap = get_submap_at( p, l );
    if( current_submap == nullptr ) {
        nulitems.clear();
        return map_stack( {
            .stack = &nulitems,
            .location = abs_pos,
            .local_origin = this,
        } );
    }

    if( can_delegate_item_mutation_to_mapbuffer( *this, abs_pos, current_submap ) ) {
        return map_stack( {
            .stack = &current_submap->get_items( l ),
            .location = abs_pos,
            .origin = &get_mapbuffer(),
        } );
    }

    return map_stack( {
        .stack = &current_submap->get_items( l ),
        .location = abs_pos,
        .local_origin = this,
    } );
}

map_stack::iterator map::i_rem( const tripoint_bub_ms &p, map_stack::const_iterator it,
                                detached_ptr<item>  *out )
{
    point_sm_ms l;
    submap *const current_submap = get_submap_at( tripoint_bub_ms( p ), l );
    if( current_submap == nullptr ) {
        return location_vector<item>::iterator();
    }

    const auto abs_pos = map_local_to_abs( *this, p );
    if( can_delegate_item_mutation_to_mapbuffer( *this, abs_pos, current_submap ) ) {
        return get_mapbuffer().erase_item( abs_pos, {
            .it = std::move( it ),
            .out = out,
            .lookup = resident_item_lookup(),
        } );
    }

    // remove from the active items cache (if it isn't there does nothing)
    current_submap->active_items.remove( *it );
    get_mapbuffer().refresh_active_item_submap_index( project_to<coords::sm>( abs_pos ),
            resident_item_lookup() );

    const auto removed_emissive = ( *it )->is_emissive();
    current_submap->update_lum_rem( l, **it );
    if( removed_emissive ) {
        get_mapbuffer().refresh_luminous_item_submap_index( project_to<coords::sm>( abs_pos ),
                resident_item_lookup() );
        invalidate_lightmap_caches();
    }

    return current_submap->get_items( l ).erase( std::move( it ), out );
}

detached_ptr<item> map::i_rem( const tripoint_bub_ms &p, item *it )
{
    point_sm_ms l;
    submap *const current_submap = get_submap_at( p, l );
    if( current_submap == nullptr ) {
        return detached_ptr<item>();
    }

    const auto abs_pos = map_local_to_abs( *this, p );
    if( can_delegate_item_mutation_to_mapbuffer( *this, abs_pos, current_submap ) ) {
        return get_mapbuffer().remove_item( abs_pos, it,
                                            resident_item_lookup() );
    }

    auto &items = current_submap->get_items( l );
    if( std::ranges::find( items, it ) == items.end() ) {
        return detached_ptr<item>();
    }

    current_submap->active_items.remove( it );
    get_mapbuffer().refresh_active_item_submap_index( project_to<coords::sm>( abs_pos ),
            resident_item_lookup() );

    const auto removed_emissive = it->is_emissive();
    current_submap->update_lum_rem( l, *it );
    if( removed_emissive ) {
        get_mapbuffer().refresh_luminous_item_submap_index( project_to<coords::sm>( abs_pos ),
                resident_item_lookup() );
        invalidate_lightmap_caches();
    }

    return items.remove( it );
}

std::vector<detached_ptr<item>> map::i_clear( const tripoint_bub_ms &p )
{
    point_sm_ms l;
    submap *const current_submap = get_submap_at( p, l );
    if( current_submap == nullptr ) {
        return {};
    }

    const auto abs_pos = map_local_to_abs( *this, p );
    if( can_delegate_item_mutation_to_mapbuffer( *this, abs_pos, current_submap ) ) {
        return get_mapbuffer().clear_items( abs_pos, resident_item_lookup() );
    }

    for( item * const &it : current_submap->get_items( l ) ) {
        // remove from the active items cache (if it isn't there does nothing)
        current_submap->active_items.remove( it );
    }
    get_mapbuffer().refresh_active_item_submap_index( project_to<coords::sm>( abs_pos ),
            resident_item_lookup() );

    const auto had_luminance = current_submap->get_lum( l ) != 0;
    current_submap->set_lum( l, 0 );
    if( had_luminance ) {
        get_mapbuffer().refresh_luminous_item_submap_index( project_to<coords::sm>( abs_pos ),
                resident_item_lookup() );
        invalidate_lightmap_caches();
    }
    return current_submap->get_items( l ).clear();
}

detached_ptr<item> map::spawn_an_item( const tripoint_bub_ms &p, detached_ptr<item> &&new_item,
                                       const int charges, const int damlevel )
{
    if( one_in( 3 ) && new_item->has_flag( flag_VARSIZE ) ) {
        new_item->set_flag( flag_FIT );
    }

    if( charges && new_item->charges > 0 ) {
        //let's fail silently if we specify charges for an item that doesn't support it
        new_item->charges = charges;
    }
    detached_ptr<item> spawned_item = item::in_its_container( std::move( new_item ) );
    if( ( spawned_item->made_of( LIQUID ) && has_flag( "SWIMMABLE", p ) ) ||
        has_flag( "DESTROY_ITEM", p ) ) {
        return detached_ptr<item>();
    }

    spawned_item->set_damage( damlevel );

    return add_item_or_charges( p, std::move( spawned_item ) );
}

float map::item_category_spawn_rate( const item &itm )
{
    const std::string &cat = itm.get_category().id.c_str();
    float spawn_rate = get_option<float>( "SPAWN_RATE_" + cat );

    // strictly search for canned foods only in the first check
    if( itm.goes_bad_after_opening( true ) ) {
        float spawn_rate_mod = get_option<float>( "SPAWN_RATE_perishables_canned" );
        spawn_rate *= spawn_rate_mod;
    } else if( itm.goes_bad() ) {
        float spawn_rate_mod = get_option<float>( "SPAWN_RATE_perishables" );
        spawn_rate *= spawn_rate_mod;
    }

    return spawn_rate > 1.0f ? roll_remainder( spawn_rate ) : spawn_rate;
}

std::vector<detached_ptr<item>> map::spawn_items( const tripoint_bub_ms &p,
                             std::vector<detached_ptr<item>> new_items )
{
    std::vector<detached_ptr<item>> ret;
    if( has_flag( "DESTROY_ITEM", p ) ) {
        return ret;
    }
    const bool swimmable = has_flag( "SWIMMABLE", p );
    for( detached_ptr<item> &new_item : new_items ) {
        if( new_item->made_of( LIQUID ) && swimmable ) {
            continue;
        }
        new_item = add_item_or_charges( p, std::move( new_item ) );
        if( new_item ) {
            ret.push_back( std::move( new_item ) );
        }
    }

    return ret;
}

void map::spawn_artifact( const tripoint_bub_ms &p )
{
    add_item_or_charges( p, item::spawn( new_artifact(), calendar::start_of_cataclysm ) );
}

void map::spawn_natural_artifact( const tripoint_bub_ms &p, artifact_natural_property prop )
{
    add_item_or_charges( p, item::spawn( new_natural_artifact( prop ),
                                         calendar::start_of_cataclysm ) );
}

void map::spawn_item( const tripoint_bub_ms &p, const itype_id &type_id,
                      const unsigned quantity, const int charges,
                      const time_point &birthday, const int damlevel )
{
    if( type_id.is_null() ) {
        return;
    }

    if( item_is_blacklisted( type_id ) ) {
        return;
    }

    // Skip spawning items in dimension-bounded out-of-bounds areas
    if( get_mapbuffer().is_outside_pocket_dimension_bounds( map_local_to_abs( *this, p ) ) ) {
        return;
    }

    for( size_t i = 0; i < quantity; i++ ) {
        // spawn the item
        detached_ptr<item> new_item = item::spawn( type_id, birthday );

        spawn_an_item( p, std::move( new_item ), charges, damlevel );
    }
}

units::volume map::max_volume( const tripoint_bub_ms &p )
{
    return i_at( p ).max_volume();
}

// total volume of all the things
units::volume map::stored_volume( const tripoint_bub_ms &p )
{
    return i_at( p ).stored_volume();
}

// free space
units::volume map::free_volume( const tripoint_bub_ms &p )
{
    return i_at( p ).free_volume();
}

detached_ptr<item> map::add_item_or_charges( const tripoint_bub_ms &pos, detached_ptr<item> &&obj,
        bool overflow )
{
    if( !obj ) {
        return std::move( obj );
    }
    if( obj->is_null() ) {
        debugmsg( "Tried to add a null item to the map" );
        return std::move( obj );
    }

    point_sm_ms local_tile;
    submap *const current_submap = get_submap_at( pos, local_tile );
    const auto abs_pos = map_local_to_abs( *this, pos );
    if( can_delegate_item_mutation_to_mapbuffer( *this, abs_pos, current_submap ) ) {
        return get_mapbuffer().add_item_or_charges( abs_pos, std::move( obj ), {
            .overflow = overflow,
            .lookup = resident_item_lookup(),
        } );
    }

    // Checks if item would not be destroyed if added to this tile
    auto valid_tile = [&]( const tripoint_bub_ms & e ) {
        // Cannot add items to dimension-bounded out-of-bounds areas or unloaded submaps
        if( get_mapbuffer().is_outside_pocket_dimension_bounds( map_local_to_abs( *this, e ) ) ) {
            return false;
        }

        // Some tiles destroy items (e.g. lava)
        if( has_flag( "DESTROY_ITEM", e ) ) {
            return false;
        }

        // Cannot drop liquids into tiles that are comprised of liquid
        if( obj->made_of( LIQUID ) && has_flag( "SWIMMABLE", e ) ) {
            return false;
        }

        return true;
    };

    // Checks if sufficient space at tile to add item
    auto valid_limits = [&]( const tripoint_bub_ms & e ) {
        return obj->volume() <= free_volume( e ) && i_at( e ).size() < MAX_ITEM_IN_SQUARE;
    };

    // Performs the actual insertion of the object onto the map
    auto place_item = [&]( const tripoint_bub_ms & tile ) {
        if( obj->count_by_charges() ) {
            for( auto &e : i_at( tile ) ) {
                // NOLINTNEXTLINE(bugprone-use-after-move)
                if( e->merge_charges( std::move( obj ) ) ) {
                    return;
                }
            }
        }

        support_dirty( tile );
        add_item( tile, std::move( obj ) );
    };

    // Some items never exist on map as a discrete item (must be contained by another item)
    if( obj->has_flag( flag_NO_DROP ) ) {
        return std::move( obj );
    }

    // If intended drop tile destroys the item then we don't attempt to overflow
    if( !valid_tile( pos ) ) {
        return std::move( obj );
    }

    if( ( !has_flag( "NOITEM", pos ) || ( has_flag( "LIQUIDCONT", pos ) && obj->made_of( LIQUID ) ) )
        && valid_limits( pos ) ) {
        // Pass map into on_drop, because this map may not be the global map object (in mapgen, for instance).
        if( obj->made_of( LIQUID ) || !obj->has_flag( flag_DROP_ACTION_ONLY_IF_LIQUID ) ) {
            if( obj->on_drop( pos, *this ) ) {
                return std::move( obj );
            }

        }
        // If tile can contain items place here...
        place_item( pos );
        return detached_ptr<item>();

    } else if( overflow ) {
        // ...otherwise try to overflow to adjacent tiles (if permitted)
        const int max_dist = 2;
        std::vector<tripoint_bub_ms> tiles = closest_points_first( pos, max_dist );
        tiles.erase( tiles.begin() ); // we already tried this position
        const int max_path_length = 4 * max_dist;
        const pathfinding_settings setting( 0, max_dist, max_path_length, 0, false, true, false, false,
                                            false );
        for( const auto &e : tiles ) {
            if( get_mapbuffer().is_outside_pocket_dimension_bounds( map_local_to_abs( *this, e ) ) ) {
                continue;
            }
            //must be a path to the target tile
            if( route( pos, e, setting ).empty() ) {
                continue;
            }
            if( obj->made_of( LIQUID ) || !obj->has_flag( flag_DROP_ACTION_ONLY_IF_LIQUID ) ) {
                if( obj->on_drop( e, *this ) ) {
                    return std::move( obj );
                }
            }

            if( !valid_tile( e ) || !valid_limits( e ) ||
                has_flag( "NOITEM", e ) || has_flag( "SEALED", e ) ) {
                continue;
            }
            place_item( e );
            return detached_ptr<item>();
        }
    }

    // failed due to lack of space at target tile (+/- overflow tiles)
    return std::move( obj );
}

void map::add_item( const tripoint_bub_ms &p, detached_ptr<item> &&new_item )
{
    if( !new_item ) {
        return;
    }
    point_sm_ms l;
    submap *const current_submap = get_submap_at( p, l );
    if( current_submap == nullptr ) {
        return;
    }

    const auto abs_pos = map_local_to_abs( *this, p );
    if( can_delegate_item_mutation_to_mapbuffer( *this, abs_pos, current_submap ) ) {
        get_mapbuffer().add_item( abs_pos, std::move( new_item ),
                                  resident_item_lookup() );
        return;
    }

    // Process foods when they are added to the map, here instead of add_item_at()
    // to avoid double processing food and corpses during active item processing.
    if( new_item->is_food() ) {
        new_item = item::process( std::move( new_item ), nullptr, p, false );
        if( !new_item ) {
            return;
        }
    }

    if( new_item->made_of( LIQUID ) && has_flag( "SWIMMABLE", p ) ) {
        return;
    }

    if( has_flag( "DESTROY_ITEM", p ) ) {
        return;
    }

    if( new_item->has_flag( flag_ACT_IN_FIRE ) && get_field( p, fd_fire ) != nullptr ) {
        if( new_item->has_flag( flag_BOMB ) && new_item->is_transformable() ) {
            //Convert a bomb item into its transformable version, e.g. incendiary grenade -> active incendiary grenade
            new_item->convert( dynamic_cast<const iuse_transform *>
                               ( new_item->type->get_use( "transform" )->get_actor_ptr() )->target );
        }
        new_item->activate();
    }

    if( new_item->is_map() && !new_item->has_var( "reveal_map_center_omt" ) ) {
        new_item->set_var( "reveal_map_center_omt", project_to<coords::omt>( map_local_to_abs( *this,
                           p ) ) );
    }

    current_submap->is_uniform = false;
    invalidate_max_populated_zlev( p.z() );

    const auto adds_luminance = new_item->is_emissive();
    current_submap->update_lum_add( l, *new_item );
    if( adds_luminance ) {
        get_mapbuffer().refresh_luminous_item_submap_index( project_to<coords::sm>( abs_pos ),
                resident_item_lookup() );
        invalidate_lightmap_caches();
    }
    if( new_item->needs_processing() ) {
        current_submap->active_items.add( *new_item );
        get_mapbuffer().refresh_active_item_submap_index( project_to<coords::sm>( abs_pos ),
                resident_item_lookup() );
    }

    new_item->on_map_placement( *this, p );

    current_submap->get_items( l ).push_back( std::move( new_item ) );
    return;
}

detached_ptr<item> map::water_from( const tripoint_bub_ms &p )
{
    if( has_flag( "SALT_WATER", p ) ) {
        return item::spawn( "salt_water", calendar::start_of_cataclysm, item::INFINITE_CHARGES );
    }

    const ter_id terrain_id = ter( p );
    if( terrain_id == t_sewage ) {
        detached_ptr<item> ret = item::spawn( "water_sewage", calendar::start_of_cataclysm,
                                              item::INFINITE_CHARGES );
        ret->poison = rng( 1, 7 );
        return ret;
    }


    // iexamine::water_source requires a valid liquid from this function.
    if( terrain_id.obj().examine == &iexamine::water_source ) {
        detached_ptr<item> ret = item::spawn( "water", calendar::start_of_cataclysm,
                                              item::INFINITE_CHARGES );
        int poison_chance = 0;
        if( terrain_id.obj().has_flag( TFLAG_DEEP_WATER ) ) {
            if( terrain_id.obj().has_flag( TFLAG_CURRENT ) ) {
                poison_chance = 20;
            } else {
                poison_chance = 4;
            }
        } else {
            if( terrain_id.obj().has_flag( TFLAG_CURRENT ) ) {
                poison_chance = 10;
            } else {
                poison_chance = 3;
            }
        }
        if( one_in( poison_chance ) ) {
            ret->poison = rng( 1, 4 );
        }
        return ret;
    }
    if( furn( p ).obj().examine == &iexamine::water_source ) {
        return item::spawn( "water", calendar::start_of_cataclysm, item::INFINITE_CHARGES );
    }
    if( furn( p ).obj().examine == &iexamine::clean_water_source ||
        terrain_id.obj().examine == &iexamine::clean_water_source ) {
        return item::spawn( "water_clean", calendar::start_of_cataclysm, item::INFINITE_CHARGES );
    }
    if( furn( p ).obj().examine == &iexamine::liquid_source ) {
        // Terrains have no "provides_liquids" to work with generic source
        return item::spawn( furn( p ).obj().provides_liquids, calendar::turn, item::INFINITE_CHARGES );
    }
    return detached_ptr<item>();
}

void map::make_inactive( item &loc )
{
    const auto local_pos = loc.bub_pos();
    point_sm_ms l;
    submap *const current_submap = get_submap_at( local_pos, l );
    if( current_submap == nullptr ) {
        return;
    }

    const auto abs_pos = map_local_to_abs( *this, local_pos );
    if( can_delegate_item_mutation_to_mapbuffer( *this, abs_pos, current_submap ) ) {
        get_mapbuffer().make_item_inactive( abs_pos, loc,
                                            resident_item_lookup() );
        return;
    }

    // remove from the active items cache (if it isn't there does nothing)
    current_submap->active_items.remove( &loc );
    get_mapbuffer().refresh_active_item_submap_index( project_to<coords::sm>( abs_pos ),
            resident_item_lookup() );
}

void map::make_active( item &loc )
{
    item *target = &loc;

    // Trust but verify, don't let stinking callers set items active when they shouldn't be.
    if( !target->needs_processing() ) {
        return;
    }
    const auto local_pos = loc.bub_pos();
    point_sm_ms l;
    submap *const current_submap = get_submap_at( local_pos, l );
    if( current_submap == nullptr ) {
        return;
    }

    const auto abs_pos = loc.abs_pos();
    if( can_delegate_item_mutation_to_mapbuffer( *this, abs_pos, current_submap ) ) {
        get_mapbuffer().make_item_active( abs_pos, loc, resident_item_lookup() );
        return;
    }

    current_submap->active_items.add( *target );
    get_mapbuffer().refresh_active_item_submap_index( project_to<coords::sm>( abs_pos ),
            resident_item_lookup() );
}

void map::update_lum( item &loc, bool add )
{
    item *target = &loc;

    // if the item is not emissive, do nothing
    if( !target->is_emissive() ) {
        return;
    }

    const auto local_pos = loc.bub_pos();
    point_sm_ms l;
    submap *const current_submap = get_submap_at( local_pos, l );
    if( current_submap == nullptr ) {
        return;
    }

    const auto abs_pos = map_local_to_abs( *this, local_pos );
    if( can_delegate_item_mutation_to_mapbuffer( *this, abs_pos, current_submap ) ) {
        get_mapbuffer().update_item_lum( abs_pos, loc, {
            .add_luminance = add,
            .lookup = resident_item_lookup(),
        } );
        return;
    }

    if( add ) {
        current_submap->update_lum_add( l, *target );
    } else {
        current_submap->update_lum_rem( l, *target );
    }
    get_mapbuffer().refresh_luminous_item_submap_index( project_to<coords::sm>( abs_pos ),
            resident_item_lookup() );
    invalidate_lightmap_caches();
}

static bool process_map_items( item *item_ref, const tripoint_bub_ms &location,
                               const temperature_flag flag )
{
    ZoneScopedN( "process_map_items" );
    return item_ref->attempt_detach( [&location, &flag]( detached_ptr<item> &&it ) {
        return item::process( std::move( it ), nullptr, location, false, flag );
    } );
}

static auto vehicle_item_needs_recharge( const item &it ) -> bool
{
    return it.ammo_capacity() > it.ammo_remaining() ||
           ( it.type->battery && it.type->battery->max_capacity > it.energy_remaining() );
}

static auto process_vehicle_items( vehicle &cur_veh ) -> void
{
    for( const cargo_recharge_target &entry : cur_veh.get_cargo_recharge_targets() ) {
        if( !entry.target || !vehicle_item_needs_recharge( *entry.target ) ) {
            continue;
        }

        const auto recharge_part_idx = cur_veh.part_with_feature( entry.cargo_part, VPFLAG_RECHARGE, true );
        if( recharge_part_idx < 0 ) {
            continue;
        }

        auto &target = *entry.target;
        auto &recharge_part = cur_veh.part( recharge_part_idx );
        if( !recharge_part.enabled || recharge_part.removed || recharge_part.is_broken() ) {
            continue;
        }

        auto power = recharge_part.info().bonus;
        while( power >= 1000 || x_in_y( power, 1000 ) ) {
            const auto missing = cur_veh.discharge_battery( 1, false );
            if( missing > 0 ) {
                return;
            }
            if( target.is_battery() ) {
                target.mod_energy( 1_kJ );
            } else {
                target.ammo_set( itype_battery, target.ammo_remaining() + 1 );
            }
            power -= 1000;
        }
    }
}

std::vector<tripoint_abs_sm> map::check_submap_active_item_consistency()
{
    std::vector<tripoint_abs_sm> result;

    // Direction 1: every in-grid submap with active items should be in the set.
    // Lazy-border submaps are intentionally excluded: they are pre-loaded for
    // shift performance but never registered in submaps_with_active_items.
    const auto &active_item_submaps = get_mapbuffer().get_submaps_with_active_items();
    for( const auto &view : active_submaps_.submaps() ) {
        const submap &sm = view.get_submap();
        if( sm.active_items.empty() ) {
            continue;
        }
        if( !active_item_submaps.contains( sm.position() ) ) {
            result.push_back( sm.position() );
        }
    }

    // Direction 2: every entry in the set should point to a loaded submap with active items.
    mapbuffer &buf = get_mapbuffer();
    for( const tripoint_abs_sm &p : active_item_submaps ) {
        submap *s = buf.lookup_submap_in_memory( p );
        if( s == nullptr || s->active_items.empty() ) {
            result.push_back( p );
        }
    }

    return result;
}

void map::process_items()
{
    // Defer explosion drains during processing: an item here can be detached but
    // still in-stack, and a re-entrant drain would re-detonate it forever (#9696).
    explosion_handler::scoped_drain_deferral defer_explosion_drains;

    auto total_active_items = int64_t{ 0 };
    auto total_rottable_active_items = int64_t{ 0 };

    // Process vehicle items from in-bubble submaps via per-z-level caches.
    // Out-of-bubble vehicle items are handled by batch_turns_items().
    {
        ZoneScopedN( "process_items_vehicles" );
        std::set<submap *> veh_submaps;
        for( int z = -OVERMAP_DEPTH; z <= OVERMAP_HEIGHT; ++z ) {
            for( vehicle *veh : get_cache( z ).vehicle_list ) {
                submap *sm = get_mapbuffer().lookup_submap_in_memory(
                                 veh->abs_sm_pos );
                if( sm != nullptr ) {
                    veh_submaps.insert( sm );
                }
            }
        }
        std::ranges::for_each( veh_submaps, [&]( submap * sm ) {
            {
                ZoneScopedN( "process_items_count_vehicle_active_items" );
                for( const auto &veh : sm->vehicles ) {
                    if( !veh ) {
                        continue;
                    }
                    const auto counts = veh->active_items.count();
                    total_active_items += counts.total;
                    total_rottable_active_items += counts.rottable;
                }
            }
            process_items_in_vehicles( *sm );
        } );
    }
    // Snapshot because processing can add or remove active submaps.
    ZoneScopedN( "process_items_submaps" );
    std::vector<tripoint_abs_sm> submaps_with_active_items_copy;
    {
        ZoneScopedN( "process_items_snapshot_active_submaps" );
        const auto &active_item_submaps = get_mapbuffer().get_submaps_with_active_items();
        submaps_with_active_items_copy = std::vector<tripoint_abs_sm>(
                                             active_item_submaps.begin(), active_item_submaps.end() );
    }
    auto active_items = std::vector<item *> {};
    {
        ZoneScopedN( "process_items_scan_active_submaps" );
        for( const tripoint_abs_sm &abs_pos : submaps_with_active_items_copy ) {
            if( !submap_loader.is_simulated( bound_dimension_, tripoint_abs_sm( abs_pos ) ) ) {
                continue;
            }
            const auto local_pos = abs_to_map_local( *this, abs_pos );
            submap *const current_submap = get_mapbuffer().lookup_submap_in_memory( abs_pos );
            if( current_submap == nullptr ) {
                continue;
            }
            if( !current_submap->active_items.empty() ) {
                {
                    ZoneScopedN( "process_items_count_active_items" );
                    const auto counts = current_submap->active_items.count();
                    total_active_items += counts.total;
                    total_rottable_active_items += counts.rottable;
                }
                process_items_in_submap( *current_submap, local_pos, active_items );
            }
        }
    }
    TracyPlot( "Total Active Items", total_active_items );
    TracyPlot( "Total Rottable Active Items", total_rottable_active_items );
}

auto map::process_items_in_submap( submap &current_submap, const tripoint_bub_sm &gridp,
                                   std::vector<item *> &active_items ) -> void
{
    ZoneScopedN( "process_items_in_submap" );
    // Get a COPY of the active item list for this submap.
    // If more are added as a side effect of processing, they are ignored this turn.
    // If they are destroyed before processing, they don't get processed.
    {
        ZoneScopedN( "process_items_copy_active_items" );
        current_submap.active_items.get_for_processing( active_items );
    }
    const point grid_offset( gridp.x() * SEEX, gridp.y() * SEEY );
    {
        ZoneScopedN( "process_items_active_items" );
        for( item *&active_item_ref : active_items ) {
            if( !active_item_ref || !active_item_ref->is_loaded() ) {
                // The item was destroyed, so skip it.
                continue;
            }

            const auto map_location = active_item_ref->bub_pos();
            const auto flag = rot::temp::for_location( *this, *active_item_ref );
            process_map_items( active_item_ref, map_location, flag );
        }
    }
}

void map::process_items_in_vehicles( submap &current_submap )
{
    // a copy, important if the vehicle list changes because a
    // vehicle got destroyed by a bomb (an active item!), this list
    // won't change, but veh_in_nonant will change.
    std::vector<vehicle *> vehicles;
    vehicles.reserve( current_submap.vehicles.size() );
    for( const auto &veh : current_submap.vehicles ) {
        vehicles.push_back( veh.get() );
    }
    for( auto &cur_veh : vehicles ) {
        if( !current_submap.contains_vehicle( cur_veh ) ) {
            // vehicle not in the vehicle list of the nonant, has been
            // destroyed (or moved to another nonant?)
            // Can't be sure that it still exists, so skip it
            continue;
        }

        process_items_in_vehicle( *cur_veh, current_submap );
    }
}

void map::process_items_in_vehicle( vehicle &cur_veh, submap &current_submap )
{
    const bool engine_heater_is_on = cur_veh.has_part( "E_HEATER", true ) && cur_veh.engine_on;
    for( const vpart_reference &vp : cur_veh.get_any_parts( VPFLAG_FLUIDTANK ) ) {
        vp.part().process_contents( vp.pos(), engine_heater_is_on );
    }

    // If there is nothing to process, skip the expensive cargo-part collection.
    if( cur_veh.active_items.empty() && !cur_veh.has_cargo_recharge ) {
        return;
    }

    if( cur_veh.has_cargo_recharge ) {
        process_vehicle_items( cur_veh );
    }

    if( cur_veh.active_items.empty() ) {
        return;
    }

    auto cargo_parts = cur_veh.get_parts_including_carried( VPFLAG_CARGO );

    for( item *active_item_ref : cur_veh.active_items.get_for_processing() ) {
        if( cargo_parts.empty() ) {
            return;
        }
        const auto it = std::ranges::find_if( cargo_parts, [&]( const vpart_reference & part ) {
            return active_item_ref->abs_pos() == cur_veh.mount_to_abs( part.mount() );
        } );

        if( it == cargo_parts.end() ) {
            continue; // Can't find a cargo part matching the active item.
        }
        const item &target = *active_item_ref;
        // Find the cargo part and coordinates corresponding to the current active item.
        const auto item_loc = it->pos();
        auto flag = temperature_flag::TEMP_NORMAL;
        if( target.is_food() || target.is_food_container() || target.is_corpse() ) {
            flag = rot::temp::for_part( cur_veh, it->part_index(), engine_heater_is_on );
        }
        if( !process_map_items( active_item_ref, item_loc, flag ) ) {
            // If the item was NOT destroyed, we can skip the remainder,
            // which handles fallout from the vehicle being damaged.
            continue;
        }

        // item does not exist anymore, might have been an exploding bomb,
        // check if the vehicle is still valid (does exist)
        if( !current_submap.contains_vehicle( &cur_veh ) ) {
            // Nope, vehicle is not in the vehicle list of the submap,
            // it might have moved to another submap (unlikely)
            // or be destroyed, anyway it does not need to be processed here
            return;
        }

        // Vehicle still valid, reload the list of cargo parts,
        // the list of cargo parts might have changed (imagine a part with
        // a low index has been removed by an explosion, all the other
        // parts would move up to fill the gap).
        cargo_parts = cur_veh.get_any_parts( VPFLAG_CARGO );
    }
}

// Crafting/item finding functions

// Note: this is called quite a lot when drawing tiles
// Console build has the most expensive parts optimized out
bool map::sees_some_items( const tripoint_bub_ms &p, const Creature &who ) const
{
    // Can only see items if there are any items.
    return has_items( p ) && could_see_items( p, who.bub_pos() );
}

bool map::sees_some_items( const tripoint_bub_ms &p, const tripoint_bub_ms &from ) const
{
    return has_items( p ) && could_see_items( p, from );
}

bool map::could_see_items( const tripoint_bub_ms &p, const Creature &who ) const
{
    return could_see_items( p, who.bub_pos() );
}

bool map::could_see_items( const tripoint_bub_ms &p, const tripoint_bub_ms &from ) const
{
    static const std::string container_string( "CONTAINER" );
    const bool container = has_flag_ter_or_furn( container_string, p );
    const bool sealed = has_flag_ter_or_furn( TFLAG_SEALED, p );
    if( sealed && container ) {
        // never see inside of sealed containers
        return false;
    }
    if( container ) {
        // can see inside of containers if adjacent or
        // on top of the container
        return ( std::abs( p.x() - from.x() ) <= 1 &&
                 std::abs( p.y() - from.y() ) <= 1 &&
                 std::abs( p.z() - from.z() ) <= 1 );
    }
    return true;
}

bool map::has_items( const tripoint_bub_ms &p ) const
{
    point_sm_ms l;
    submap *const current_submap = get_submap_at( tripoint_bub_ms( p ), l );
    if( current_submap == nullptr ) {
        return false;
    }

    return !current_submap->get_items( l ).empty();
}

template <typename Stack>
std::vector<detached_ptr<item>> use_amount_stack( Stack stack, const itype_id &type, int &quantity,
                             const std::function<bool( const item & )> &filter )
{
    std::vector<detached_ptr<item>> ret;

    stack.remove_top_items_with( [&quantity, &filter, &type, &ret]( detached_ptr<item> &&it ) {
        if( quantity <= 0 ) {
            return std::move( it );
        }
        detached_ptr<item> new_it = item::use_amount( std::move( it ), type, quantity, ret, filter );
        // NOLINTNEXTLINE(bugprone-use-after-move)
        if( it && !new_it ) {
            ret.push_back( std::move( it ) );
        }
        return new_it;
    } );
    return ret;
}

std::vector<detached_ptr<item>> map::use_amount_square( const tripoint_bub_ms &p,
                             const itype_id &type,
                             int &quantity, const std::function<bool( const item & )> &filter )
{
    std::vector<detached_ptr<item>> ret;
    // Handle infinite map sources.
    detached_ptr<item> water = water_from( p );
    if( water && water->typeId() == type ) {
        ret.push_back( std::move( water ) );
        quantity = 0;
        return ret;
    }

    if( const std::optional<vpart_reference> vp = veh_at( p ).part_with_feature( "CARGO", true ) ) {
        std::vector<detached_ptr<item>> tmp = use_amount_stack( vp->vehicle().get_items( vp->part_index() ),
                                              type,
                                              quantity, filter );
        ret.insert( ret.end(), std::make_move_iterator( tmp.begin() ),
                    std::make_move_iterator( tmp.end() ) );
    }
    std::vector<detached_ptr<item>> tmp = use_amount_stack( i_at( p ), type, quantity, filter );
    ret.insert( ret.end(), std::make_move_iterator( tmp.begin() ),
                std::make_move_iterator( tmp.end() ) );
    return ret;
}

std::vector<detached_ptr<item>> map::use_amount( const tripoint_bub_ms &origin, const int range,
                             const itype_id &type,
                             int &quantity, const std::function<bool( const item & )> &filter )
{
    std::vector<detached_ptr<item>> ret;
    for( int radius = 0; radius <= range && quantity > 0; radius++ ) {
        for( const tripoint_bub_ms &p : points_in_radius( origin, radius ) ) {
            if( rl_dist( origin, p ) >= radius ) {
                std::vector<detached_ptr<item>> tmp = use_amount_square( p, type, quantity, filter );
                ret.insert( ret.end(), std::make_move_iterator( tmp.begin() ),
                            std::make_move_iterator( tmp.end() ) );
            }
        }
    }
    return ret;
}

template <typename Stack>
std::vector<detached_ptr<item>> use_charges_from_stack( Stack stack, const itype_id &type,
                             int &quantity,
                             const tripoint_bub_ms &pos, const std::function<bool( const item & )> &filter )
{

    std::vector<detached_ptr<item>> ret;

    stack.remove_top_items_with( [&quantity, &filter, &type, &pos, &ret]( detached_ptr<item> &&it ) {
        if( quantity <= 0 || it->made_of( LIQUID ) ) {
            return std::move( it );
        }
        detached_ptr<item> new_it = item::use_charges( std::move( it ), type, quantity, ret, pos, filter );
        // NOLINTNEXTLINE(bugprone-use-after-move)
        if( it && !new_it ) {
            ret.push_back( std::move( it ) );
        }
        return new_it;
    } );
    return ret;
}

static void use_charges_from_furn( const furn_t &f, const itype_id &type, int &quantity,
                                   map *m, const tripoint_bub_ms &p, std::vector<detached_ptr<item>> &ret,
                                   const std::function<bool( const item & )> &filter )
{
    if( m->has_flag( "LIQUIDCONT", p ) ) {
        auto item_list = m->i_at( p );
        auto current_item = item_list.begin();
        for( ; current_item != item_list.end(); ++current_item ) {
            // looking for a liquid that matches
            if( filter( **current_item ) && ( *current_item )->made_of( LIQUID ) &&
                type == ( *current_item )->typeId() ) {

                if( ( *current_item )->charges - quantity > 0 ) {
                    ret.push_back( ( *current_item )->split( quantity ) );
                    // All the liquid needed was found, no other sources will be needed
                    quantity = 0;
                } else {
                    // The liquid copy in ret already contains how much was available
                    // The leftover quantity returned will check other sources
                    quantity -= ( *current_item )->charges;
                    // Remove liquid item from the world
                    detached_ptr<item> det;
                    item_list.erase( current_item, &det );
                    ret.push_back( std::move( det ) );
                }
                return;
            }
        }
    }

    const std::vector<itype> item_list = f.crafting_pseudo_item_types();
    static const flag_id json_flag_USES_GRID_POWER( flag_USES_GRID_POWER );
    for( const itype &itt : item_list ) {
        if( itt.has_flag( json_flag_USES_GRID_POWER ) ) {
            const auto abspos = map_local_to_abs( *m, p );
            auto &grid = get_distribution_grid_tracker().grid_at( abspos );
            detached_ptr<item> furn_item = item::spawn( itt.get_id(), calendar::start_of_cataclysm,
                                           grid.get_resource() );
            int initial_quantity = quantity;
            if( filter( *furn_item ) ) {
                item::use_charges( std::move( furn_item ), type, quantity, ret, p );
                // That quantity math thing is atrocious. Punishment for the int& "argument".
                grid.mod_resource( quantity - initial_quantity );
            }
        } else if( itt.tool && !itt.tool->ammo_id.empty() ) {
            const itype_id ammo = ammotype( *itt.tool->ammo_id.begin() )->default_ammotype();
            if( itt.tool->subtype != type && type != ammo && itt.get_id() != type ) {
                continue;
            }
            auto stack = m->i_at( p );
            auto iter = std::ranges::find_if( stack,
            [ammo]( const item * const & i ) {
                return i->typeId() == ammo;
            } );
            if( iter != stack.end() ) {

                ( *iter )->attempt_detach( [&filter, &ammo, &quantity, &ret, &p]( detached_ptr<item> &&it ) {
                    if( filter( *it ) ) {
                        return item::use_charges( std::move( it ), ammo, quantity, ret, p );
                    }
                    return std::move( it );
                } );
            }
        }
    }
}

std::vector<detached_ptr<item>> map::use_charges( const tripoint_bub_ms &origin, const int range,
                             const itype_id &type, int &quantity,
                             const std::function<bool( const item & )> &filter )
{
    std::vector<detached_ptr<item>> ret;

    // populate a grid of spots that can be reached
    std::vector<tripoint_bub_ms> reachable_pts;

    if( range <= 0 ) {
        reachable_pts.push_back( origin );
    } else {
        reachable_flood_steps( reachable_pts, origin, range, 1, 100 );
    }

    // We prefer infinite map sources where available, so search for those
    // first
    for( const tripoint_bub_ms &p : reachable_pts ) {
        // Handle infinite map sources.
        detached_ptr<item> water = water_from( p );
        if( water && water->typeId() == type ) {
            water->charges = quantity;
            ret.push_back( std::move( water ) );
            quantity = 0;
            return ret;
        }
    }

    for( const tripoint_bub_ms &p : reachable_pts ) {
        if( has_furn( p ) ) {
            use_charges_from_furn( furn( p ).obj(), type, quantity, this, p, ret, filter );
            if( quantity <= 0 ) {
                return ret;
            }
        }

        if( accessible_items( p ) ) {
            std::vector<detached_ptr<item>> tmp = use_charges_from_stack( i_at( p ), type, quantity, p,
                                                  filter );
            ret.insert( ret.end(), std::make_move_iterator( tmp.begin() ),
                        std::make_move_iterator( tmp.end() ) );
            if( quantity <= 0 ) {
                return ret;
            }
        }

        const optional_vpart_position vp = veh_at( p );
        if( !vp ) {
            continue;
        }

        const std::optional<vpart_reference> crafterpart = vp.part_with_feature( "CRAFTER", true );
        const std::optional<vpart_reference> faupart = vp.part_with_feature( "FAUCET", true );
        const std::optional<vpart_reference> autoclavepart = vp.part_with_feature( "AUTOCLAVE", true );
        const std::optional<vpart_reference> cargo = vp.part_with_feature( "CARGO", true );

        auto drain_vehicle_pseudo_item = [&ret, &quantity, &type]( vehicle & veh,
        const itype_id & drain_type ) -> bool {
            const auto drained = veh.drain( drain_type, quantity );
            quantity -= drained;
            if( drained <= 0 )
            {
                return quantity == 0;
            }
            auto tmp = item::spawn( type, calendar::turn );
            tmp->charges = drained;
            ret.push_back( std::move( tmp ) );
            return quantity == 0;
        };

        if( crafterpart ) {
            for( const auto &id : crafterpart->info().craftertools() ) {
                if( type == id && drain_vehicle_pseudo_item( crafterpart->vehicle(), itype_battery ) ) {
                    return ret;
                }
            }
        }
        if( faupart ) { // we have a faucet, now to see what to drain
            // TODO: Handle water poison when crafting starts respecting it
            if( drain_vehicle_pseudo_item( faupart->vehicle(), type ) ) {
                return ret;
            }
        }

        if( autoclavepart ) { // we have an autoclave, now to see what to drain
            auto ftype = itype_id::NULL_ID();

            if( type == itype_autoclave ) {
                ftype = itype_battery;
            }

            if( drain_vehicle_pseudo_item( autoclavepart->vehicle(), ftype ) ) {
                return ret;
            }
        }

        if( cargo ) {
            std::vector<detached_ptr<item>> tmp =
                                             use_charges_from_stack( cargo->vehicle().get_items( cargo->part_index() ), type, quantity,
                                                     tripoint_bub_ms( p ),
                                                     filter );
            ret.insert( ret.end(), std::make_move_iterator( tmp.begin() ),
                        std::make_move_iterator( tmp.end() ) );
            if( quantity <= 0 ) {
                return ret;
            }
        }
    }

    return ret;
}

bool map::can_see_trap_at( const tripoint_bub_ms &p, const Character &c ) const
{
    return tr_at( p ).can_see( p, c );
}

const trap &map::tr_at( const tripoint_bub_ms &p ) const
{
    if( get_mapbuffer().is_outside_pocket_dimension_bounds( map_local_to_abs( *this, p ) ) ) {
        return tr_null.obj();
    }

    point_sm_ms l;
    submap *const current_submap = get_submap_at( tripoint_bub_ms( p ), l );

    if( current_submap == nullptr ) {
        return tr_null.obj();
    }

    if( current_submap->get_ter( l ).obj().trap != tr_null ) {
        return current_submap->get_ter( l ).obj().trap.obj();
    }

    return current_submap->get_trap( l ).obj();
}

partial_con *map::partial_con_at( const tripoint_bub_ms &p )
{
    const auto abs_pos = map_local_to_abs( *this, p );
    if( get_mapbuffer().is_outside_pocket_dimension_bounds( abs_pos ) ) {
        return nullptr;
    }
    return get_mapbuffer().partial_con_at( abs_pos, resident_item_lookup() );
}

void map::partial_con_remove( const tripoint_bub_ms &p )
{
    const auto abs_pos = map_local_to_abs( *this, p );
    if( get_mapbuffer().is_outside_pocket_dimension_bounds( abs_pos ) ) {
        return;
    }
    get_mapbuffer().partial_con_remove( abs_pos, resident_item_lookup() );
}

void map::partial_con_set( const tripoint_bub_ms &p, std::unique_ptr<partial_con> con )
{
    const auto abs_pos = map_local_to_abs( *this, p );
    if( get_mapbuffer().is_outside_pocket_dimension_bounds( abs_pos ) ) {
        return;
    }
    get_mapbuffer().partial_con_set( abs_pos, std::move( con ), resident_item_lookup() );
}

void map::trap_set( const tripoint_bub_ms &p, const trap_id &type )
{
    get_mapbuffer().set_trap( map_local_to_abs( *this, p ), type, resident_item_lookup() );
}

void map::disarm_trap( const tripoint_bub_ms &p )
{
    const trap &tr = tr_at( p );
    if( tr.is_null() ) {
        debugmsg( "Tried to disarm a trap where there was none (%d %d %d)", p.x(), p.y(), p.z() );
        return;
    }

    const int tSkillLevel = g->u.get_skill_level( skill_traps );
    const int diff = tr.get_difficulty();
    const int tReward = diff + tr.get_avoidance();
    int roll = rng( tSkillLevel, 4 * tSkillLevel );

    // Some traps are not actual traps. Skip the rolls, different message and give the option to grab it right away.
    if( tr.get_avoidance() == 0 && tr.get_difficulty() == 0 ) {
        add_msg( _( "The %s is taken down." ), tr.name() );
        tr.on_disarmed( *this, p );
        return;
    }

    ///\EFFECT_PER increases chance of disarming trap

    ///\EFFECT_DEX increases chance of disarming trap

    ///\EFFECT_TRAPS increases chance of disarming trap
    while( ( rng( 5, 20 ) < g->u.per_cur || rng( 1, 20 ) < g->u.dex_cur ) && roll < 50 ) {
        roll++;
    }
    if( roll >= diff ) {
        add_msg( _( "You disarm the trap!" ) );
        const int morale_buff = tr.get_avoidance() * 0.4 + tr.get_difficulty() + rng( 0, 4 );
        g->u.rem_morale( MORALE_FAILURE );
        g->u.add_morale( MORALE_ACCOMPLISHMENT, morale_buff, 40 );
        tr.on_disarmed( *this, p );
        if( diff > 1.25 * tSkillLevel ) { // failure might have set off trap
            g->u.practice( skill_traps, tReward );
        }
    } else if( roll >= diff * .8 ) {
        add_msg( _( "You fail to disarm the trap." ) );
        const int morale_debuff = -rng( 6, 18 );
        g->u.rem_morale( MORALE_ACCOMPLISHMENT );
        g->u.add_morale( MORALE_FAILURE, morale_debuff, -40 );
        if( diff > 1.25 * tSkillLevel ) {
            g->u.practice( skill_traps, tReward / 2 );
        }
    } else {
        add_msg( m_bad, _( "You fail to disarm the trap, and you set it off!" ) );
        const int morale_debuff = -rng( 12, 24 );
        g->u.rem_morale( MORALE_ACCOMPLISHMENT );
        g->u.add_morale( MORALE_FAILURE, morale_debuff, -40 );
        tr.trigger( p, &g->u );
        g->u.practice( skill_traps, tReward / 4 );
    }
    g->u.mod_moves( -100 );
}

void map::remove_trap( const tripoint_bub_ms &p )
{
    get_mapbuffer().set_trap( map_local_to_abs( *this, p ), tr_null, resident_item_lookup() );
}
/*
 * Get wrapper for all fields at xyz
 */
const field &map::field_at( const tripoint_bub_ms &p ) const
{
    if( get_mapbuffer().is_outside_pocket_dimension_bounds( map_local_to_abs( *this, p ) ) ) {
        nulfield = field();
        return nulfield;
    }

    point_sm_ms l;
    submap *const current_submap = get_submap_at( tripoint_bub_ms( p ), l );

    if( current_submap == nullptr ) {
        nulfield = field();
        return nulfield;
    }

    return current_submap->get_field( l );
}

/*
 * As above, except not const
 */
field &map::field_at( const tripoint_bub_ms &p )
{
    const auto abs_pos = map_local_to_abs( *this, p );
    if( get_mapbuffer().is_outside_pocket_dimension_bounds( abs_pos ) ) {
        nulfield = field();
        return nulfield;
    }

    field *const current_field = get_mapbuffer().get_field( abs_pos, resident_item_lookup() );
    if( current_field == nullptr ) {
        nulfield = field();
        return nulfield;
    }

    return *current_field;
}

time_duration map::mod_field_age( const tripoint_bub_ms &p, const field_type_id &type,
                                  const time_duration &offset )
{
    return set_field_age( p, type, offset, true );
}

int map::mod_field_intensity( const tripoint_bub_ms &p, const field_type_id &type,
                              const int offset )
{
    return set_field_intensity( p, type, offset, true );
}

time_duration map::set_field_age( const tripoint_bub_ms &p, const field_type_id &type,
                                  const time_duration &age, const bool isoffset )
{
    return get_mapbuffer().set_field_age( map_local_to_abs( *this, p ), {
        .type = type,
        .age = age,
        .isoffset = isoffset,
        .lookup = resident_item_lookup(),
    } ).value_or( -1_turns );
}

/*
 * set intensity of field type at point, creating if not present, removing if intensity is 0
 * returns resulting intensity, or 0 for not present
 */
int map::set_field_intensity( const tripoint_bub_ms &p, const field_type_id &type,
                              const int new_intensity,
                              bool isoffset )
{
    return get_mapbuffer().set_field_intensity( map_local_to_abs( *this, p ), {
        .type = type,
        .intensity = new_intensity,
        .isoffset = isoffset,
        .lookup = resident_item_lookup(),
    } ).value_or( 0 );
}

time_duration map::get_field_age( const tripoint_bub_ms &p, const field_type_id &type ) const
{
    auto field_ptr = field_at( p ).find_field( type );
    return field_ptr == nullptr ? -1_turns : field_ptr->get_field_age();
}

int map::get_field_intensity( const tripoint_bub_ms &p, const field_type_id &type ) const
{
    auto field_ptr = field_at( p ).find_field( type );
    return ( field_ptr == nullptr ? 0 : field_ptr->get_field_intensity() );
}


bool map::has_field_at( const tripoint_bub_ms &p, bool check_bounds )
{
    if( check_bounds &&
        get_mapbuffer().is_outside_pocket_dimension_bounds( map_local_to_abs( *this, p ) ) ) {
        return false;
    }
    return get_mapbuffer().has_field_at( map_local_to_abs( *this, p ), resident_item_lookup() );
}

field_entry *map::get_field( const tripoint_bub_ms &p, const field_type_id &type )
{
    return get_mapbuffer().get_field_entry( map_local_to_abs( *this, p ), type,
                                            resident_item_lookup() );
}

bool map::dangerous_field_at( const tripoint_bub_ms &p )
{
    for( auto &pr : field_at( p ) ) {
        auto &fd = pr.second;
        if( fd.is_dangerous() ) {
            return true;
        }
    }
    return false;
}

bool map::add_field( const tripoint_bub_ms &p, const field_type_id &type_id, int intensity,
                     const time_duration &age, bool hit_player )
{
    const auto added = get_mapbuffer().add_field( map_local_to_abs( *this, p ), {
        .type = type_id,
        .intensity = intensity,
        .age = age,
        .lookup = resident_item_lookup(),
    } );
    if( !added ) {
        return false;
    }

    if( hit_player ) {
        Character &player_character = get_player_character();
        if( g != nullptr && this == &get_map() && p == player_character.bub_pos() ) {
            //Hit the player with the field if it spawned on top of them.
            creature_in_field( player_character );
        }
    }

    return true;
}

void map::remove_field( const tripoint_bub_ms &p, const field_type_id &field_to_remove )
{
    get_mapbuffer().remove_field( map_local_to_abs( *this, p ), field_to_remove,
                                  resident_item_lookup() );
}

void map::add_splatter( const field_type_id &type, const tripoint_bub_ms &where, int intensity )
{
    if( !type.id() || intensity <= 0 ) {
        return;
    }
    if( type.obj().is_splattering ) {
        if( const optional_vpart_position vp = veh_at( where ) ) {
            vehicle *const veh = &vp->vehicle();
            // Might be -1 if all the vehicle's parts at where are marked for removal
            const int part = veh->part_displayed_at( vp->mount() );
            if( part != -1 ) {
                veh->part( part ).blood += 200 * std::min( intensity, 3 ) / 3;
                return;
            }
        }
    }
    mod_field_intensity( where, type, intensity );
}

void map::add_splatter_trail( const field_type_id &type, const tripoint_bub_ms &from,
                              const tripoint_bub_ms &to )
{
    if( !type.id() ) {
        return;
    }
    auto trail = line_to( from, to );
    int remainder = trail.size();
    tripoint_bub_ms last_point = from;
    for( tripoint_bub_ms &elem : trail ) {
        add_splatter( type, elem );
        remainder--;
        if( obstructed_by_vehicle_rotation( last_point, elem ) ) {
            if( one_in( 2 ) ) {
                elem.x() = last_point.x();
                add_splatter( type, elem, remainder );
            } else {
                elem.y() = last_point.y();
                add_splatter( type, elem, remainder );
            }
            return;
        }
        if( impassable( elem ) ) { // Blood splatters stop at walls.
            add_splatter( type, elem, remainder );
            return;
        }
        last_point = elem;
    }
}

void map::add_splash( const field_type_id &type, const tripoint_bub_ms &center, int radius,
                      int intensity )
{
    if( !type.id() ) {
        return;
    }
    // TODO: use Bresenham here and take obstacles into account
    for( const tripoint_bub_ms &pnt : points_in_radius( center, radius ) ) {
        if( trig_dist( pnt, center ) <= radius && !one_in( intensity ) ) {
            add_splatter( type, pnt );
        }
    }
}

computer *map::computer_at( const tripoint_bub_ms &p )
{
    return get_mapbuffer().get_computer( map_local_to_abs( *this, p ), resident_item_lookup() );
}

void map::update_submap_active_item_status( const tripoint_bub_ms &p )
{
    get_mapbuffer().refresh_active_item_submap_index( map_local_to_abs( *this, p ),
            resident_item_lookup() );
}


auto map::update_visibility_cache( const int zlev,
                                   const std::function<void()> &while_gpu_pending ) -> void
{
    ZoneScopedN( "update_visibility_cache" );
    const auto player_pos = g->u.bub_pos();
#if defined( CATA_SDL )
    if( cata_compute::uses_sdl_gpu_compute() ) {
        SDL_GPUDevice *const gpu_device = cata_gpu::get_device();
        if( gpu_device == nullptr ) {
            debugmsg( "SDL_GPU visibility is required, but no GPU device is available" );
            return;
        }
        const auto &visibility_cache_for_residency = get_cache_ref( zlev );
        const auto visibility_cache_x = visibility_cache_for_residency.cache_x;
        const auto visibility_cache_y = visibility_cache_for_residency.cache_y;
        if( !cata_gpu::resident_lighting_ready_for_visibility( {
        .device = gpu_device,
        .cache_x = visibility_cache_x,
        .cache_y = visibility_cache_y,
        .z_count = OVERMAP_LAYERS,
    } ) ) {
            build_map_cache( zlev );
            if( !cata_gpu::resident_lighting_ready_for_visibility( {
            .device = gpu_device,
            .cache_x = visibility_cache_x,
            .cache_y = visibility_cache_y,
            .z_count = OVERMAP_LAYERS,
        } ) ) {
                debugmsg( "SDL_GPU visibility residency bootstrap failed; see debug.log for details" );
                return;
            }
        }
    }
#endif
    visibility_variables_cache = make_visibility_variables( zlev );

#if defined( CATA_SDL )
    if( cata_compute::uses_sdl_gpu_compute() ) {
        SDL_GPUDevice *const gpu_device = cata_gpu::get_device();
        auto const mark_overmap_seen_from_visibility = [this]( const level_cache & vc_cache ) {
            auto sm_squares_seen = std::vector<int>( static_cast<size_t>( my_MAPSIZE ) * my_MAPSIZE, 0 );
            for( const auto x : std::views::iota( 0, vc_cache.cache_x ) ) {
                for( const auto y : std::views::iota( 0, vc_cache.cache_y ) ) {
                    const auto ll = vc_cache.visibility_cache[vc_cache.idx( x, y )];
                    sm_squares_seen[( x / SEEX ) * my_MAPSIZE + y / SEEY] +=
                        ( ll == lit_level::BRIGHT || ll == lit_level::LIT );
                }
            }
            for( const auto p : bubble_submaps() ) {
                if( sm_squares_seen[p.x() * my_MAPSIZE + p.y()] > 36 ) {
                    const auto abs_sm = bub_to_abs( p );
                    const auto abs_omt( project_to<coords::omt>( abs_sm ) );
                    get_overmapbuffer( bound_dimension_ ).set_seen( tripoint_abs_omt( abs_omt.xy(), 0 ), true );
                }
            }
        };

        auto visibility_download_levels = std::vector<int> {};
        std::ranges::copy( std::views::iota( -OVERMAP_DEPTH, OVERMAP_HEIGHT + 1 ),
                           std::back_inserter( visibility_download_levels ) );
        const auto rebuild_seen_cache = m_last_seen_cache_origin != player_pos;
        const auto gpu_visibility_work = cata_gpu::begin_gpu_visibility( gpu_device, {
            .m = this,
            .download_levels = &visibility_download_levels,
            .zlev = zlev,
            .player_x = player_pos.x(),
            .player_y = player_pos.y(),
            .player_zlev = player_pos.z(),
            .g_light_level = visibility_variables_cache.g_light_level,
            .u_clairvoyance = visibility_variables_cache.u_clairvoyance,
            .u_unimpaired_range = visibility_variables_cache.u_unimpaired_range,
            .vision_threshold = visibility_variables_cache.vision_threshold,
            .visibility_scale_factor = visibility_variables_cache.visibility_scale_factor,
            .detail_range = visibility_variables_cache.detail_range,
            .vision_block_mask = vision_transparency_block_mask(),
            .rebuild_seen_cache = rebuild_seen_cache,
        } );
        if( gpu_visibility_work.id == 0 ) {
            debugmsg( "SDL_GPU visibility dispatch failed; see debug.log for details" );
            return;
        }
        if( while_gpu_pending ) {
            while_gpu_pending();
        }
        const auto gpu_visibility_ok = cata_gpu::finish_gpu_visibility( gpu_device, gpu_visibility_work );
        if( !gpu_visibility_ok ) {
            debugmsg( "SDL_GPU visibility completion failed; see debug.log for details" );
            return;
        }
        if( rebuild_seen_cache ) {
            auto &origin_cache = get_cache( player_pos.z() );
            std::fill( origin_cache.camera_cache.begin(), origin_cache.camera_cache.end(), 0.0f );
            m_last_seen_cache_origin = player_pos;
        }
        mark_visibility_caches_clean();
        mark_overmap_seen_from_visibility( get_cache_ref( zlev ) );
        return;
    }
#endif

    ( void )while_gpu_pending;

    auto sm_squares_seen = std::vector<int>( static_cast<size_t>( my_MAPSIZE ) * my_MAPSIZE, 0 );

    const int min_z = -OVERMAP_DEPTH;
    const int max_z = OVERMAP_HEIGHT;
    const auto max_delta_z = std::max( std::abs( min_z - player_pos.z() ),
                                       std::abs( max_z - player_pos.z() ) );
    const auto &reference_cache = get_cache_ref( zlev );
    const auto *const distance_table = trigdist ?
    &get_rl_dist_lookup_table( rl_dist_lookup_table_dimensions{
        .max_dx = reference_cache.cache_x - 1,
        .max_dy = reference_cache.cache_y - 1,
        .max_dz = max_delta_z,
        .trigdist = trigdist,
    } ) :
        nullptr;

    for( const auto z : std::views::iota( min_z, max_z + 1 ) ) {

        level_cache &vc_cache = get_cache( z );
        auto &visibility_cache = vc_cache.visibility_cache;
        const auto dz = std::abs( z - player_pos.z() );

        // Fill visibility_cache.  apparent_light_at is read-only per tile.
        if( parallel_enabled && parallel_map_cache ) {
            parallel_for( 0, vc_cache.cache_x, [&]( int x ) {
                const auto dx = std::abs( x - player_pos.x() );
                for( const auto y : std::views::iota( 0, vc_cache.cache_y ) ) {
                    const auto dy = std::abs( y - player_pos.y() );
                    const auto dist = distance_table != nullptr ?
                                      distance_table->distance_3d( dx, dy, dz ) :
                                      std::max( { dx, dy, dz } );
                    visibility_cache[vc_cache.idx( x, y )] =
                        apparent_light_at( tripoint_bub_ms{ x, y, z }, visibility_variables_cache, dist );
                }
            } );
            // Overmap discovery accumulation: serial, reads from the parallel-filled cache.
            // Kept separate because sm_squares_seen is not thread-safe to write from workers.
            if( z == zlev ) {
                for( const auto x : std::views::iota( 0, vc_cache.cache_x ) ) {
                    for( const auto y : std::views::iota( 0, vc_cache.cache_y ) ) {
                        const auto ll = visibility_cache[vc_cache.idx( x, y )];
                        sm_squares_seen[( x / SEEX ) * my_MAPSIZE + y / SEEY] +=
                            ( ll == lit_level::BRIGHT || ll == lit_level::LIT );
                    }
                }
            }
        } else {
            // Serial path: merge visibility fill and overmap discovery into one pass,
            // avoiding a second full scan of the cache at the player's z-level.
            const bool count_discovery = ( z == zlev );
            for( const auto x : std::views::iota( 0, vc_cache.cache_x ) ) {
                const auto dx = std::abs( x - player_pos.x() );
                for( const auto y : std::views::iota( 0, vc_cache.cache_y ) ) {
                    const auto dy = std::abs( y - player_pos.y() );
                    const auto dist = distance_table != nullptr ?
                                      distance_table->distance_3d( dx, dy, dz ) :
                                      std::max( { dx, dy, dz } );
                    const auto ll =
                        apparent_light_at( tripoint_bub_ms{ x, y, z }, visibility_variables_cache, dist );
                    visibility_cache[vc_cache.idx( x, y )] = ll;
                    if( count_discovery ) {
                        sm_squares_seen[( x / SEEX ) * my_MAPSIZE + y / SEEY] +=
                            ( ll == lit_level::BRIGHT || ll == lit_level::LIT );
                    }
                }
            }
        }
    }

    for( const auto p : bubble_submaps() ) {
        if( sm_squares_seen[p.x() * my_MAPSIZE + p.y()] > 36 ) { // 25% of the submap is visible
            const auto abs_sm = map_local_to_abs( *this, p );
            const auto abs_omt( project_to<coords::omt>( abs_sm ) );
            get_overmapbuffer( bound_dimension_ ).set_seen( tripoint_abs_omt( abs_omt.xy(), 0 ), true );
        }
    }

    // Mark all z-levels touched by this run as clean so subsequent draws within
    // the same turn can skip the rebuild entirely.
    mark_visibility_caches_clean();
}

const visibility_variables &map::get_visibility_variables_cache() const
{
    return visibility_variables_cache;
}

visibility_type map::get_visibility( const lit_level ll,
                                     const visibility_variables &cache ) const
{
    switch( ll ) {
        case lit_level::DARK:
            // can't see this square at all
            if( cache.u_is_boomered ) {
                return VIS_BOOMER_DARK;
            } else {
                return VIS_DARK;
            }
        case lit_level::BRIGHT_ONLY:
            // can only tell that this square is bright
            if( cache.u_is_boomered ) {
                return VIS_BOOMER;
            } else {
                return VIS_LIT;
            }

        case lit_level::LOW:
        // low light, square visible in monochrome
        case lit_level::LIT:
        // normal light
        case lit_level::BRIGHT:
            // bright light
            return VIS_CLEAR;
        case lit_level::BLANK:
        case lit_level::MEMORIZED:
            return VIS_HIDDEN;
    }
    return VIS_HIDDEN;
}

static bool has_memory_at( const tripoint_bub_ms &p )
{
    if( g->u.should_show_map_memory() ) {
        int t = g->u.get_memorized_symbol( bub_to_abs( p ) );
        return t != 0;
    }
    return false;
}

static int get_memory_at( const tripoint_bub_ms &p )
{
    if( g->u.should_show_map_memory() ) {
        return g->u.get_memorized_symbol( bub_to_abs( p ) );
    }
    return ' ';
}

void map::draw( const catacurses::window &w, const tripoint_bub_ms &center )
{
    // We only need to draw anything if we're not in tiles mode.
    if( is_draw_tiles_mode() ) {
        return;
    }

    g->reset_light_level();

    update_visibility_cache( center.z() );
    const visibility_variables &cache = get_visibility_variables_cache();

    const level_cache &draw_lc = get_cache_ref( center.z() );
    const auto &visibility_cache = draw_lc.visibility_cache;

    int wnd_h = getmaxy( w );
    int wnd_w = getmaxx( w );
    const auto offs = center - tripoint_rel_ms( wnd_w / 2, wnd_h / 2, 0 );

    // Map memory should be at least the size of the view range
    // so that new tiles can be memorized, and at least the size of the terminal
    // since displayed area may be bigger than view range.
    const auto min_mm_reg = point_bub_ms(
                                std::min( 0, offs.x() ),
                                std::min( 0, offs.y() )
                            );
    const auto max_mm_reg = point_bub_ms(
                                std::max( g_mapsize_x, offs.x() + wnd_w ),
                                std::max( g_mapsize_y, offs.y() + wnd_h )
                            );
    g->u.prepare_map_memory_region(
        map_local_to_abs( *this, tripoint_bub_ms( min_mm_reg, center.z() ) ),
        map_local_to_abs( *this, tripoint_bub_ms( max_mm_reg, center.z() ) )
    );

    const auto draw_background = [&]( const tripoint_bub_ms & p ) {
        int sym = ' ';
        nc_color col = c_black;
        if( has_memory_at( p ) ) {
            sym = get_memory_at( p );
            col = c_brown;
        }
        wputch( w, col, sym );
    };

    const auto draw_vision_effect = [&]( const visibility_type vis ) -> bool {
        int sym = '#';
        nc_color col;
        switch( vis )
        {
            case VIS_LIT:
                // can only tell that this square is bright
                col = c_light_gray;
                break;
            case VIS_BOOMER:
                col = c_pink;
                break;
            case VIS_BOOMER_DARK:
                col = c_magenta;
                break;
            default:
                return false;
        }
        wputch( w, col, sym );
        return true;
    };

    drawsq_params params = drawsq_params().memorize( true );
    for( int wy = 0; wy < wnd_h; wy++ ) {
        for( int wx = 0; wx < wnd_w; wx++ ) {
            wmove( w, point( wx, wy ) );
            const tripoint_bub_ms p = offs + tripoint_rel_ms( wx, wy, 0 );
            if( !inbounds( p ) ) {
                draw_background( p );
                continue;
            }

            const lit_level lighting = visibility_cache[draw_lc.idx( p.x(), p.y() )];
            const visibility_type vis = get_visibility( lighting, cache );

            if( draw_vision_effect( vis ) ) {
                continue;
            }

            if( vis == VIS_HIDDEN || vis == VIS_DARK ) {
                draw_background( p );
                continue;
            }

            const maptile curr_maptile = maptile_at_internal( tripoint_bub_ms( p ) );
            params
            .low_light( lighting == lit_level::LOW )
            .bright_light( lighting == lit_level::BRIGHT );
            if( draw_maptile( w, p, curr_maptile, params ) ) {
                continue;
            }
            const maptile tile_below = maptile_at_internal( tripoint_bub_ms( p ) - tripoint_above );
            draw_from_above( w, tripoint_bub_ms( p.xy(), p.z() - 1 ), tile_below, params );
        }
    }

    // Memorize off-screen tiles
    half_open_rectangle<point_bub_ms> display( offs.xy(), offs.xy() + point_rel_ms( wnd_w, wnd_h ) );
    drawsq_params mm_params = drawsq_params().memorize( true ).output( false );
    for( int y = 0; y < draw_lc.cache_y; y++ ) {
        for( int x = 0; x < draw_lc.cache_x; x++ ) {
            const tripoint_bub_ms p( x, y, center.z() );
            if( display.contains( p.xy() ) ) {
                // Have been memorized during display loop
                continue;
            }

            const lit_level lighting = visibility_cache[draw_lc.idx( p.x(), p.y() )];
            const visibility_type vis = get_visibility( lighting, cache );

            if( vis != VIS_CLEAR ) {
                continue;
            }

            const maptile curr_maptile = maptile_at_internal( tripoint_bub_ms( p ) );
            mm_params
            .low_light( lighting == lit_level::LOW )
            .bright_light( lighting == lit_level::BRIGHT );

            draw_maptile( w, p, curr_maptile, mm_params );
        }
    }
}

void map::drawsq( const catacurses::window &w, const tripoint_bub_ms &p,
                  const drawsq_params &params ) const
{
    // If we are in tiles mode, the only thing we want to potentially draw is a highlight
    if( is_draw_tiles_mode() ) {
        if( params.highlight() ) {
            g->draw_highlight( p );
        }
        return;
    }

    if( !inbounds( p ) ) {
        return;
    }

    const auto view_center = params.center();
    const int k = p.x() + getmaxx( w ) / 2 - view_center.x();
    const int j = p.y() + getmaxy( w ) / 2 - view_center.y();
    wmove( w, point( k, j ) );

    const maptile tile = maptile_at( tripoint_bub_ms( p ) );
    if( draw_maptile( w, p, tile, params ) ) {
        return;
    }

    tripoint_bub_ms below( p.xy(), p.z() - 1 );
    const maptile tile_below = maptile_at( below );
    draw_from_above( w, below, tile_below, params );
}

// a check to see if the lower floor needs to be rendered in tiles
bool map::dont_draw_lower_floor( const tripoint_bub_ms &p )
{
    return p.z() <= -OVERMAP_DEPTH || !( has_flag( TFLAG_NO_FLOOR, p ) ||
                                         has_flag( TFLAG_Z_TRANSPARENT, p ) );
}

bool map::draw_maptile( const catacurses::window &w, const tripoint_bub_ms &p,
                        const maptile &curr_maptile, const drawsq_params &params ) const
{
    drawsq_params param = params;
    nc_color tercol;
    const ter_t &curr_ter = curr_maptile.get_ter_t();
    const furn_t &curr_furn = curr_maptile.get_furn_t();
    const trap &curr_trap = curr_maptile.get_trap().obj();
    const field &curr_field = curr_maptile.get_field();
    int sym;
    bool hi = false;
    bool graf = false;
    bool draw_item_sym = false;

    int terrain_sym;
    if( curr_ter.has_flag( TFLAG_AUTO_WALL_SYMBOL ) ) {
        terrain_sym = determine_wall_corner( p );
    } else {
        terrain_sym = curr_ter.symbol();
    }

    if( curr_furn.id ) {
        sym = curr_furn.symbol();
        tercol = curr_furn.color();
    } else {
        sym = terrain_sym;
        tercol = curr_ter.color();
    }
    if( curr_ter.has_flag( TFLAG_SWIMMABLE ) && curr_ter.has_flag( TFLAG_DEEP_WATER ) &&
        !g->u.is_underwater() ) {
        param.show_items( false ); // Can only see underwater items if WE are underwater
    }
    // If there's a trap here, and we have sufficient perception, draw that instead
    if( curr_trap.can_see( p, g->u ) ) {
        tercol = curr_trap.color;
        if( curr_trap.sym == '%' ) {
            switch( rng( 1, 5 ) ) {
                case 1:
                    sym = '*';
                    break;
                case 2:
                    sym = '0';
                    break;
                case 3:
                    sym = '8';
                    break;
                case 4:
                    sym = '&';
                    break;
                case 5:
                    sym = '+';
                    break;
            }
        } else {
            sym = curr_trap.sym;
        }
    }
    if( curr_field.field_count() > 0 ) {
        const field_type_id &fid = curr_field.displayed_field_type();
        const field_entry *fe = curr_field.find_field( fid );
        const auto field_symbol = fid->get_symbol();
        if( field_symbol == "&" || fe == nullptr ) {
            // Do nothing, a '&' indicates invisible fields.
        } else if( field_symbol == "*" ) {
            // A random symbol.
            switch( rng( 1, 5 ) ) {
                case 1:
                    sym = '*';
                    break;
                case 2:
                    sym = '0';
                    break;
                case 3:
                    sym = '8';
                    break;
                case 4:
                    sym = '&';
                    break;
                case 5:
                    sym = '+';
                    break;
            }
        } else {
            // A field symbol '%' indicates the field should not hide
            // items/terrain. When the symbol is not '%' it will
            // hide items (the color is still inverted if there are items,
            // but the tile symbol is not changed).
            // draw_item_sym indicates that the item symbol should be used
            // even if sym is not '.'.
            // As we don't know at this stage if there are any items
            // (that are visible to the player!), we always set the symbol.
            // If there are items and the field does not hide them,
            // the code handling items will override it.
            draw_item_sym = ( field_symbol == "'%" );
            // If field display_priority is > 1, and the field is set to hide items,
            //draw the field as it obscures what's under it.
            if( ( field_symbol != "%" && fid.obj().priority > 1 ) || ( field_symbol != "%" &&
                    sym == '.' ) )  {
                // default terrain '.' and
                // non-default field symbol -> field symbol overrides terrain
                sym = field_symbol[0];
            }
            tercol = fe->color();
        }
    }

    // TODO: change the local variable sym to std::string and use it instead of this hack.
    // Currently this are different variables because terrain/... uses int as symbol type and
    // item now use string. Ideally they should all be strings.
    std::string item_sym;

    // If there are items here, draw those instead
    if( param.show_items() && curr_maptile.get_item_count() > 0 && sees_some_items( p, g->u ) ) {
        // if there's furniture/terrain/trap/fields (sym!='.')
        // and we should not override it, then only highlight the square
        if( sym != '.' && sym != '%' && !draw_item_sym ) {
            hi = true;
        } else {
            // otherwise override with the symbol of the last item
            item_sym = curr_maptile.get_uppermost_item().symbol();
            if( !draw_item_sym ) {
                tercol = curr_maptile.get_uppermost_item().color();
            }
            if( curr_maptile.get_item_count() > 1 ) {
                param.highlight( !param.highlight() );
            }
        }
    }

    int memory_sym = sym;
    int veh_part = 0;
    const vehicle *veh = veh_at_internal( tripoint_bub_ms( p ), veh_part );
    if( veh != nullptr ) {
        const auto part_face = tileray( veh->part_display_direction( veh_part ) );
        sym = special_symbol( part_face.dir_symbol( veh->part_sym( veh_part ) ) );
        tercol = veh->part_color( veh_part );
        item_sym.clear(); // clear the item symbol so `sym` is used instead.

        if( !veh->forward_velocity() && !veh->player_in_control( g->u ) ) {
            memory_sym = sym;
        }
    }

    if( param.memorize() && check_and_set_seen_cache( p ) ) {
        g->u.memorize_symbol( map_local_to_abs( *this, p ), memory_sym );
    }

    // If there's graffiti here, change background color
    if( curr_maptile.has_graffiti() ) {
        graf = true;
    }

    const auto u_vision = g->u.get_vision_modes();
    if( u_vision[BOOMERED] ) {
        tercol = c_magenta;
    } else if( u_vision[NV_GOGGLES] ) {
        tercol = param.bright_light() ? c_white : c_light_green;
    } else if( param.low_light() ) {
        tercol = c_dark_gray;
    } else if( u_vision[DARKNESS] ) {
        tercol = c_dark_gray;
    }

    if( param.highlight() ) {
        tercol = invert_color( tercol );
    } else if( hi ) {
        tercol = hilite( tercol );
    } else if( graf ) {
        tercol = red_background( tercol );
    }

    if( item_sym.empty() && sym == ' ' ) {
        if( p.z() <= -OVERMAP_DEPTH || !curr_ter.has_flag( TFLAG_NO_FLOOR ) ) {
            // Print filler symbol
            sym = ' ';
            tercol = c_black;
        } else {
            // Draw tile underneath this one instead
            return false;
        }
    }

    if( params.output() ) {
        if( item_sym.empty() ) {
            wputch( w, tercol, sym );
        } else {
            wprintz( w, tercol, item_sym );
        }
    }
    return true;
}

void map::draw_from_above( const catacurses::window &w, const tripoint_bub_ms &p,
                           const maptile &curr_tile, const drawsq_params &params ) const
{
    static const int AUTO_WALL_PLACEHOLDER = 2; // this should never appear as a real symbol!

    nc_color tercol = c_dark_gray;
    int sym = ' ';

    const ter_t &curr_ter = curr_tile.get_ter_t();
    const furn_t &curr_furn = curr_tile.get_furn_t();
    int part_below;
    const vehicle *veh;
    if( curr_furn.has_flag( TFLAG_SEEN_FROM_ABOVE ) ) {
        sym = curr_furn.symbol();
        tercol = curr_furn.color();
    } else if( curr_furn.movecost < 0 ) {
        sym = '.';
        tercol = curr_furn.color();
    } else if( ( veh = veh_at_internal( tripoint_bub_ms( p ), part_below ) ) != nullptr ) {
        const int roof = veh->roof_at_part( part_below );
        const int displayed_part = roof >= 0 ? roof : part_below;
        const auto part_face = tileray( veh->part_display_direction( part_below, roof >= 0 ) );
        sym = special_symbol( part_face.dir_symbol( veh->part_sym( displayed_part, true ) ) );
        tercol = ( roof >= 0 ||
                   vpart_position( const_cast<vehicle &>( *veh ),
                                   part_below ).obstacle_at_part() ) ? c_light_gray : c_light_gray_cyan;
    } else if( curr_ter.has_flag( TFLAG_SEEN_FROM_ABOVE ) ) {
        if( curr_ter.has_flag( TFLAG_AUTO_WALL_SYMBOL ) ) {
            sym = AUTO_WALL_PLACEHOLDER;
        } else if( curr_ter.has_flag( TFLAG_RAMP ) ) {
            sym = '>';
        } else {
            sym = curr_ter.symbol();
        }
        tercol = curr_ter.color();
    } else if( curr_ter.movecost == 0 ) {
        sym = '.';
        tercol = curr_ter.color();
    } else if( !curr_ter.has_flag( TFLAG_NO_FLOOR ) ) {
        sym = '.';
        if( curr_ter.color() != c_cyan ) {
            // Need a special case here, it doesn't cyanize well
            tercol = cyan_background( curr_ter.color() );
        } else {
            tercol = c_black_cyan;
        }
    } else {
        sym = curr_ter.symbol();
        tercol = curr_ter.color();
    }

    if( sym == AUTO_WALL_PLACEHOLDER ) {
        sym = determine_wall_corner( p );
    }

    const auto u_vision = g->u.get_vision_modes();
    if( u_vision[BOOMERED] ) {
        tercol = c_magenta;
    } else if( u_vision[NV_GOGGLES] ) {
        tercol = params.bright_light() ? c_white : c_light_green;
    } else if( params.low_light() ) {
        tercol = c_dark_gray;
    } else if( u_vision[DARKNESS] ) {
        tercol = c_dark_gray;
    }

    if( params.highlight() ) {
        tercol = invert_color( tercol );
    }

    if( params.output() ) {
        wputch( w, tercol, sym );
    }
}

bool map::sees( const tripoint_bub_ms &F, const tripoint_bub_ms &T, const int range ) const
{
    int dummy = 0;
    return sees( F, T, range, dummy );
}

/**
 * This one is internal-only, we don't want to expose the slope tweaking ickiness outside the map class.
 **/
bool map::sees( const tripoint_bub_ms &F, const tripoint_bub_ms &T, const int range,
                int &bresenham_slope ) const
{
    if( ( range >= 0 && range < rl_dist( F, T ) ) ||
        !inbounds( T ) ||
        !inbounds( F ) ) {
        bresenham_slope = 0;
        return false; // Out of range!
    }
    // Cannonicalize the order of the tripoints so the cache is reflexive.
    const tripoint_bub_ms &min = F < T ? F : T;
    const tripoint_bub_ms &max = !( F < T ) ? F : T;
    // Pack two tripoints into one int64_t: 29 bits each (12 x + 12 y + 5 z).
    // Handles coordinates up to 4095 — safe for g_mapsize up to ~340.
    auto pack_tp = []( const tripoint_bub_ms & p ) -> int64_t {
        return ( static_cast<int64_t>( p.x() ) & 0xFFF ) << 17 |
                ( static_cast<int64_t>( p.y() ) & 0xFFF ) <<  5 |
                ( static_cast<int64_t>( p.z() + OVERMAP_DEPTH ) & 0x1F );
    };
    const int64_t key = ( pack_tp( min ) << 29 ) | pack_tp( max );
    // P-6 / PERF-LOSS-1: shared_lock for the cache lookup so concurrent readers
    // don't serialize against each other.  The ray trace runs fully unlocked.
    const auto slot_idx = std::hash<int64_t> {}( key ) & ( vision_cache_slots - 1 );
    {
        std::shared_lock<std::shared_mutex> lock( *skew_vision_cache_mutex );
        const auto &slot = skew_vision_cache[slot_idx];
        if( slot.key == key && slot.value >= 0 ) {
            return slot.value > 0;
        }
    }

    bool visible = true;

    // Ugly `if` for now
    if( F.z() == T.z() ) {

        auto last_point = F.xy();
        // Please someone make bresenham work with typed points, I'm running out of willpower
        bresenham( F.xy().raw(), T.xy().raw(), bresenham_slope,
        [this, &visible, &T, &last_point]( const point & new_point ) {
            // Exit before checking the last square, it's still visible even if opaque.
            if( new_point.x == T.x() && new_point.y == T.y() ) {
                return false;
            }
            if( !this->is_transparent( tripoint_bub_ms( point_bub_ms( new_point ), T.z() ) ) ||
                obscured_by_vehicle_rotation( tripoint_bub_ms( last_point, T.z() ),
                                              tripoint_bub_ms( point_bub_ms( new_point ), T.z() ) ) ) {
                visible = false;
                return false;
            }
            last_point = point_bub_ms( new_point );
            return true;
        } );
        {
            std::unique_lock<std::shared_mutex> lock( *skew_vision_cache_mutex );
            skew_vision_cache[slot_idx] = { key, static_cast<char>( visible ? 1 : 0 ) };
        }
        return visible;
    }

    auto last_point = F;
    bresenham( F.raw(), T.raw(), bresenham_slope, 0,
    [this, &visible, &T, &last_point]( const tripoint & new_point ) {
        // Exit before checking the last square if it's not a vertical transition,
        // it's still visible even if opaque.
        if( new_point == T.raw() && last_point.z() == T.z() ) {
            return false;
        }

        // TODO: Allow transparent floors (and cache them!)
        if( new_point.z == last_point.z() ) {
            if( !this->is_transparent( tripoint_bub_ms( new_point ) ) ||
                obscured_by_vehicle_rotation( last_point, tripoint_bub_ms( new_point ) ) ) {
                visible = false;
                return false;
            }
        } else {
            const int max_z = std::max( new_point.z, last_point.z() );
            if( ( has_floor( tripoint_bub_ms{ new_point.x, new_point.y, max_z }, true ) ||
                  !is_transparent( tripoint_bub_ms{ new_point.x, new_point.y, last_point.z() } ) ) &&
                ( has_floor( {last_point.xy(), max_z}, true ) ||
                  !is_transparent( {last_point.xy(), new_point.z} ) ) ) {
                visible = false;
                return false;
            }
        }

        last_point = tripoint_bub_ms( new_point );
        return true;
    } );
    {
        std::unique_lock<std::shared_mutex> lock( *skew_vision_cache_mutex );
        skew_vision_cache[slot_idx] = { key, static_cast<char>( visible ? 1 : 0 ) };
    }
    return visible;
}

int map::obstacle_coverage( const tripoint_bub_ms &loc1, const tripoint_bub_ms &loc2 ) const
{
    // Can't hide if you are standing on furniture, or non-flat slowing-down terrain tile.
    if( furn( loc2 ).obj().id || ( move_cost( loc2 ) > 2 && !has_flag_ter( TFLAG_FLAT, loc2 ) ) ) {
        return 0;
    }
    const point_bub_ms a( std::abs( loc1.x() - loc2.x() ) * 2, std::abs( loc1.y() - loc2.y() ) * 2 );
    int offset = std::min( a.x(), a.y() ) - ( std::max( a.x(), a.y() ) / 2 );
    tripoint_bub_ms obstaclepos;
    bresenham( loc2.raw(), loc1.raw(), offset, 0, [&obstaclepos]( const tripoint & new_point ) {
        // Only adjacent tile between you and enemy is checked for cover.
        obstaclepos = tripoint_bub_ms( new_point );
        return false;
    } );
    if( const auto obstacle_f = furn( obstaclepos ) ) {
        return obstacle_f->coverage;
    }
    if( const auto vp = veh_at( obstaclepos ) ) {
        if( vp->obstacle_at_part() ) {
            return 60;
        } else if( !vp->part_with_feature( VPFLAG_AISLE, true ) ) {
            return 45;
        }
    }
    return ter( obstaclepos )->coverage;
}

int map::coverage( const tripoint_bub_ms &p ) const
{
    if( const auto obstacle_f = furn( p ) ) {
        return obstacle_f->coverage;
    }
    if( const auto vp = veh_at( p ) ) {
        if( vp->obstacle_at_part() ) {
            return 60;
        } else if( !vp->part_with_feature( VPFLAG_AISLE, true ) &&
                   !vp->part_with_feature( "PROTRUSION", true ) ) {
            return 45;
        }
    }
    return ter( p )->coverage;
}

// This method tries a bunch of initial offsets for the line to try and find a clear one.
// Basically it does, "Find a line from any point in the source that ends up in the target square".
std::vector<tripoint_bub_ms> map::find_clear_path( const tripoint_bub_ms &source,
        const tripoint_bub_ms &destination ) const
{
    // TODO: Push this junk down into the Bresenham method, it's already doing it.
    const point_rel_ms d = destination.xy() - source.xy();
    const point_rel_ms a( std::abs( d.x() ) * 2, std::abs( d.y() ) * 2 );
    const int dominant = std::max( a.x(), a.y() );
    const int minor = std::min( a.x(), a.y() );
    // This seems to be the method for finding the ideal start value for the error value.
    const int ideal_start_offset = minor - dominant / 2;
    const int start_sign = ( ideal_start_offset > 0 ) - ( ideal_start_offset < 0 );
    // Not totally sure of the derivation.
    const int max_start_offset = std::abs( ideal_start_offset ) * 2 + 1;
    for( int horizontal_offset = -1; horizontal_offset <= max_start_offset; ++horizontal_offset ) {
        int candidate_offset = horizontal_offset * start_sign;
        if( sees( source, destination, rl_dist( source, destination ), candidate_offset ) ) {
            return line_to( source, destination, candidate_offset, 0 );
        }
    }
    // If we couldn't find a clear LoS, just return the ideal one.
    return line_to( source, destination, ideal_start_offset, 0 );
}

void map::reachable_flood_steps( std::vector<tripoint_bub_ms> &reachable_pts,
                                 const tripoint_bub_ms &f,
                                 int range, const int cost_min, const int cost_max ) const
{
    if( range < 0 || !inbounds( f ) ) {
        return;
    }

    struct pq_item {
        int dist;
        int ndx;
    };
    struct pq_item_comp {
        bool operator()( const pq_item &left, const pq_item &right ) {
            return left.dist > right.dist;
        }
    };
    using PQ_type = std::priority_queue< pq_item, std::vector<pq_item>, pq_item_comp>;

    // temp buffer for grid
    const int grid_dim = range * 2 + 1;
    // init to -1 as "not visited yet"
    std::vector< int > t_grid( static_cast<size_t>( grid_dim * grid_dim ), -1 );
    const tripoint origin_offset = {range, range, 0};
    const int initial_visit_distance = range * range; // Large unreachable value

    // Fill positions that are visitable with initial_visit_distance
    for( const tripoint_bub_ms &p : points_in_radius( f, range ) ) {
        const tripoint_bub_ms tp = { p.xy(), f.z() };
        const int tp_cost = move_cost( tp );
        const auto &veh = veh_at( tp );
        const auto &veh_wall = veh.obstacle_at_part();
        // Move cost is in right bounds
        const bool bad_move_cost = tp_cost < cost_min || tp_cost > cost_max;
        // It lacks floor in terrain or in veh
        const bool no_floor = !has_floor_or_support( tp ) && ( veh_wall || !veh );
        // rejection conditions
        if( bad_move_cost || no_floor || veh_wall ) {
            continue;
        }
        // set initial cost for grid point
        auto origin_relative = tp - f;
        origin_relative += origin_offset;
        int ndx = origin_relative.x() + origin_relative.y() * grid_dim;
        if( ndx < 0 || ndx >= static_cast<int>( t_grid.size() ) ) {
            continue;
        }
        t_grid[ ndx ] = initial_visit_distance;
    }

    auto gen_neighbors = []( const pq_item & elem, int grid_dim, pq_item * neighbors ) {
        // Up to 8 neighbors
        int new_cost = elem.dist + 1;
        // *INDENT-OFF*
        int ox[8] = {
            -1, 0, 1,
            -1,    1,
            -1, 0, 1
        };
        int oy[8] = {
            -1, -1, -1,
            0,      0,
            1,  1,  1
        };
        // *INDENT-ON*

        point e( elem.ndx % grid_dim, elem.ndx / grid_dim );
        for( int i = 0; i < 8; ++i ) {
            point n( e + point( ox[i], oy[i] ) );

            int ndx = n.x + n.y * grid_dim;
            neighbors[i] = { new_cost, ndx };
        }
    };

    PQ_type pq( pq_item_comp{} );
    pq_item first_item{ 0, range + range * grid_dim };
    pq.push( first_item );
    pq_item neighbor_elems[8];

    while( !pq.empty() ) {
        const pq_item item = pq.top();
        pq.pop();

        if( item.ndx < 0 || item.ndx >= static_cast<int>( t_grid.size() ) ) {
            continue;
        }
        if( t_grid[ item.ndx ] == initial_visit_distance ) {
            t_grid[ item.ndx ] = item.dist;
            if( item.dist + 1 < range ) {
                gen_neighbors( item, grid_dim, neighbor_elems );
                for( pq_item neighbor_elem : neighbor_elems ) {
                    pq.push( neighbor_elem );
                }
            }
        }
    }
    std::vector<char> o_grid( static_cast<size_t>( grid_dim * grid_dim ), 0 );
    for( int y = 0, ndx = 0; y < grid_dim; ++y ) {
        for( int x = 0; x < grid_dim; ++x, ++ndx ) {
            if( t_grid[ ndx ] != -1 && t_grid[ ndx ] < initial_visit_distance ) {
                // set self and neighbors to 1
                for( int dy = -1; dy <= 1; ++dy ) {
                    for( int dx = -1; dx <= 1; ++dx ) {
                        int tx = dx + x;
                        int ty = dy + y;

                        if( tx >= 0 && tx < grid_dim && ty >= 0 && ty < grid_dim ) {
                            o_grid[ tx + ty * grid_dim ] = 1;
                        }
                    }
                }
            }
        }
    }

    // Now go over again to pull out all of the reachable points
    for( int y = 0, ndx = 0; y < grid_dim; ++y ) {
        for( int x = 0; x < grid_dim; ++x, ++ndx ) {
            if( o_grid[ ndx ] ) {
                auto t = f - origin_offset + tripoint_rel_ms{ x, y, 0 };
                reachable_pts.push_back( t );
            }
        }
    }
}

bool map::clear_path( const tripoint_bub_ms &f, const tripoint_bub_ms &t, const int range,
                      const int cost_min, const int cost_max ) const
{
    if( f.z() == t.z() ) {
        if( ( range >= 0 && range < rl_dist( f.xy(), t.xy() ) ) ||
            !inbounds( t ) ) {
            return false; // Out of range!
        }
        bool is_clear = true;
        auto last_point = f.xy();
        bresenham( f.xy().raw(), t.xy().raw(), 0,
        [this, &is_clear, cost_min, cost_max, &t, &last_point]( point new_point ) {
            // Exit before checking the last square, it's still reachable even if it is an obstacle.
            if( new_point.x == t.x() && new_point.y == t.y() ) {
                return false;
            }

            const int cost = this->move_cost( tripoint_bub_ms( new_point.x, new_point.y, t.z() ) );
            if( cost < cost_min || cost > cost_max ||
                obstructed_by_vehicle_rotation( tripoint_bub_ms( last_point, t.z() ),
                                                tripoint_bub_ms( point_bub_ms( new_point ), t.z() ) ) ) {
                is_clear = false;
                return false;
            }

            last_point = point_bub_ms( new_point );
            return true;
        } );
        return is_clear;
    }

    if( ( range >= 0 && range < rl_dist( f, t ) ) ||
        !inbounds( t ) ) {
        return false; // Out of range!
    }
    bool is_clear = true;
    auto last_point = f;
    bresenham( f.raw(), t.raw(), 0, 0,
    [this, &is_clear, cost_min, cost_max, t, &last_point]( const tripoint & new_point ) {
        // Exit before checking the last square, it's still reachable even if it is an obstacle.
        if( new_point == t.raw() ) {
            return false;
        }

        // We have to check a weird case where the move is both vertical and horizontal
        if( new_point.z == last_point.z() ) {
            const int cost = move_cost( tripoint_bub_ms( new_point ) );
            if( cost < cost_min || cost > cost_max ||
                obstructed_by_vehicle_rotation( last_point, tripoint_bub_ms( new_point ) ) ) {
                is_clear = false;
                return false;
            }
        } else {
            bool this_clear = false;
            const int max_z = std::max( new_point.z, last_point.z() );
            if( !has_floor_or_support( tripoint_bub_ms{ new_point.x, new_point.y, max_z } ) ) {
                const int cost = move_cost( tripoint_bub_ms{ new_point.x, new_point.y, last_point.z() } );
                if( cost > cost_min && cost < cost_max &&
                    !obstructed_by_vehicle_rotation( last_point, tripoint_bub_ms( new_point ) ) ) {
                    this_clear = true;
                }
            }

            if( !this_clear && has_floor_or_support( {last_point.xy(), max_z} ) ) {
                const int cost = move_cost( tripoint_bub_ms{ last_point.xy(), new_point.z } );
                if( cost > cost_min && cost < cost_max &&
                    !obstructed_by_vehicle_rotation( last_point, tripoint_bub_ms( new_point ) ) ) {
                    this_clear = true;
                }
            }

            if( !this_clear ) {
                is_clear = false;
                return false;
            }
        }

        last_point = tripoint_bub_ms( new_point );
        return true;
    } );
    return is_clear;
}

bool map::obstructed_by_vehicle_rotation( const tripoint_bub_ms &from,
        const tripoint_bub_ms &to ) const
{
    if( !inbounds( from ) || !inbounds( to ) ) {
        return false;
    }

    if( from.z() != to.z() ) {
        //Split it into two checks, one for each z level
        tripoint_bub_ms flattened = { from.xy(), to.z() };
        if( obstructed_by_vehicle_rotation( flattened, to ) ) {
            return true;
        }
    }

    auto delta = to.xy() - from.xy();

    const level_cache &lc = get_cache_ref( from.z() );
    const auto &cache = lc.vehicle_obstructed_cache;

    if( delta == point_rel_ms::north_west() ) {
        return cache[lc.idx( from.x(), from.y() )].nw;
    }

    if( delta == point_rel_ms::north_east() ) {
        return cache[lc.idx( from.x(), from.y() )].ne;
    }

    if( delta == point_rel_ms::south_west() ) {
        return cache[lc.idx( to.x(), to.y() )].ne;
    }
    if( delta == point_rel_ms::south_east() ) {
        return cache[lc.idx( to.x(), to.y() )].nw;
    }

    return false;
}


bool map::obscured_by_vehicle_rotation( const tripoint_bub_ms &from,
                                        const tripoint_bub_ms &to ) const
{
    if( !inbounds( from ) || !inbounds( to ) ) {
        return false;
    }

    if( from.z() != to.z() ) {
        //Split it into two checks, one for each z level
        tripoint_bub_ms flattened = { from.xy(), to.z() };
        if( obscured_by_vehicle_rotation( flattened, to ) ) {
            return true;
        }
    }

    auto delta = to.xy() - from.xy();

    const level_cache &lc = get_cache_ref( from.z() );
    const auto &cache = lc.vehicle_obscured_cache;

    if( delta == point_rel_ms::north_west() ) {
        return cache[lc.idx( from.x(), from.y() )].nw;
    }

    if( delta == point_rel_ms::north_east() ) {
        return cache[lc.idx( from.x(), from.y() )].ne;
    }

    if( delta == point_rel_ms::south_west() ) {
        return cache[lc.idx( to.x(), to.y() )].ne;
    }
    if( delta == point_rel_ms::south_east() ) {
        return cache[lc.idx( to.x(), to.y() )].nw;
    }

    return false;
}

bool map::accessible_items( const tripoint_bub_ms &t ) const
{
    return !has_flag( "SEALED", t ) || has_flag( "LIQUIDCONT", t );
}

std::vector<tripoint_bub_ms> map::get_dir_circle( const tripoint_bub_ms &f,
        const tripoint_bub_ms &t ) const
{
    std::vector<tripoint_bub_ms> circle;
    circle.resize( 8 );

    // The line below can be crazy expensive - we only take the FIRST point of it
    const std::vector<tripoint_bub_ms> line = line_to( f, t, 0, 0 );
    const std::vector<tripoint_bub_ms> spiral = closest_points_first( f, 1 );
    const std::vector<int> pos_index {1, 2, 4, 6, 8, 7, 5, 3};

    //  All possible constellations (closest_points_first goes clockwise)
    //  753  531  312  124  246  468  687  875
    //  8 1  7 2  5 4  3 6  1 8  2 7  4 5  6 3
    //  642  864  786  578  357  135  213  421

    size_t pos_offset = 0;
    for( size_t i = 1; i < spiral.size(); i++ ) {
        if( spiral[i] == line[0] ) {
            pos_offset = i - 1;
            break;
        }
    }

    for( size_t i = 1; i < spiral.size(); i++ ) {
        if( pos_offset >= pos_index.size() ) {
            pos_offset = 0;
        }

        circle[pos_index[pos_offset++] - 1] = spiral[i];
    }

    return circle;
}

void map::load( const point_abs_sm &w, const bool update_vehicle, const bool pump_events )
{
    funnel_locations_.clear();
    set_abs_sub( w );
    for( const auto p : bubble_submaps() ) {
        loadn( p, update_vehicle );
        MAPBUFFER_REGISTRY.get( bound_dimension_ ).actualize_submap( map_local_to_abs( *this, p ) );
        if( pump_events ) {
            inp_mngr.pump_events();
        }
    }
    refresh_active_submap_view();
    validate_active_submap_view_complete( "map::load" );
    reset_vehicle_cache( );

    charge_removal_blacklist::split_deferred();
}


void shift_bitset_cache( cata_dynamic_bitset &cache, int size, int multiplier,
                         const point_rel_sm &s )
{
    // sx shifts by MULTIPLIER rows, sy shifts by MULTIPLIER columns.
    int shift_amount = s.x() * multiplier + s.y() * size * multiplier;
    if( shift_amount > 0 ) {
        cache >>= static_cast<size_t>( shift_amount );
    } else if( shift_amount < 0 ) {
        cache <<= static_cast<size_t>( -shift_amount );
    }
    // Shifting in the y direction shifts in 0 values, so no additional clearing is necessary, but
    // a shift in the x direction makes values "wrap" to the next row, and they need to be zeroed.
    if( s.x() == 0 ) {
        return;
    }
    const size_t x_offset = s.x() > 0 ? static_cast<size_t>( size - multiplier ) : 0;
    for( size_t y = 0; y < static_cast<size_t>( size ); ++y ) {
        size_t y_offset = y * static_cast<size_t>( size );
        for( size_t x = 0; x < static_cast<size_t>( multiplier ); ++x ) {
            cache.reset( y_offset + x_offset + x );
        }
    }
}

static inline void shift_tripoint_set( std::set<tripoint_bub_ms> &set, const point_rel_ms &offset,
                                       const half_open_rectangle<point_bub_ms> &boundaries )
{
    std::set<tripoint_bub_ms> old_set = std::move( set );
    set.clear();
    for( const tripoint_bub_ms &pt : old_set ) {
        tripoint_bub_ms new_pt = pt + offset;
        if( boundaries.contains( new_pt.xy() ) ) {
            set.insert( new_pt );
        }
    }
}

template <typename T>
static inline void shift_tripoint_map( std::map<tripoint_bub_ms, T> &map,
                                       const point_rel_ms &offset,
                                       const half_open_rectangle<point_bub_ms> &boundaries )
{
    std::map<tripoint_bub_ms, T> old_map = std::move( map );
    map.clear();
    for( const std::pair<tripoint_bub_ms, T> &pr : old_map ) {
        tripoint_bub_ms new_pt = pr.first + offset;
        if( boundaries.contains( new_pt.xy() ) ) {
            map.emplace( new_pt, pr.second );
        }
    }
}

void map::shift_vehicle_z( vehicle &veh, int z_shift )
{
    auto src = veh.abs_sm_pos;
    auto dst = src + tripoint_rel_sm( 0, 0, z_shift );
    invalidate_lightmap_caches();
    auto dirty_vertical_vehicle_caches = [this]( const int zlev ) {
        if( !inbounds_z( zlev ) ) {
            return;
        }
        invalidate_map_cache( zlev );
    };
    dirty_vertical_vehicle_caches( src.z() );
    dirty_vertical_vehicle_caches( src.z() + 1 );
    dirty_vertical_vehicle_caches( dst.z() );
    dirty_vertical_vehicle_caches( dst.z() + 1 );

    submap *src_submap = get_mapbuffer().lookup_submap_in_memory( src );
    submap *dst_submap = get_mapbuffer().lookup_submap_in_memory( dst );

    int our_i = -1;
    for( size_t i = 0; i < src_submap->vehicles.size(); i++ ) {
        if( src_submap->vehicles[i].get() == &veh ) {
            our_i = i;
            break;
        }
    }

    if( our_i == -1 ) {
        debugmsg( "shift_vehicle [%s] failed could not find vehicle", veh.name );
        return;
    }

    for( auto &prt : veh.get_all_parts() ) {
        prt.part().z_terrain[0] -= z_shift;
        prt.part().z_terrain[1] -= z_shift;
    }

    auto src_submap_veh_it = src_submap->vehicles.begin() + our_i;
    dst_submap->vehicles.push_back( std::move( *src_submap_veh_it ) );
    src_submap->vehicles.erase( src_submap_veh_it );
    dst_submap->is_uniform = false;
    invalidate_max_populated_zlev( dst.z() );

    update_vehicle_list( dst_submap, dst.z() );

    level_cache &ch = get_cache( src.z() );
    for( const vehicle *elem : ch.vehicle_list ) {
        if( elem == &veh ) {
            ch.vehicle_list.erase( &veh );
            ch.zone_vehicles.erase( &veh );
            break;
        }
    }

    veh.abs_sm_pos = dst;
    get_mapbuffer().refresh_vehicle_footprint( &veh );
    veh.update_overmap( src );
}

void map::shift( const point_rel_sm &sp )
{
    ZoneScopedN( "map_shift" );
    // Special case of 0-shift; refresh the map
    if( sp == point_rel_sm::zero() ) {
        return; // Skip this?
    }

    if( std::abs( sp.x() ) > 1 || std::abs( sp.y() ) > 1 ) {
        debugmsg( "map::shift called with a shift of more than one submap" );
    }

    vehicle *remoteveh = g->remoteveh();

    set_abs_sub( get_abs_sub() + sp );

    g->shift_destination_preview( -project_to<coords::ms>( sp ) );

    m_last_seen_cache_origin = tripoint_bub_ms( tripoint_min );
#if defined( CATA_SDL )
    if( cata_compute::uses_sdl_gpu_compute() ) {
        auto *const gpu_device = cata_gpu::get_device();
        const auto &shift_cache = get_cache_ref( -OVERMAP_DEPTH );
        const auto gpu_residency_shifted = cata_gpu::shift_lighting_resident_inputs( {
            .device = gpu_device,
            .cache_x = shift_cache.cache_x,
            .cache_y = shift_cache.cache_y,
            .z_count = OVERMAP_LAYERS,
            .shift_x_submaps = sp.x(),
            .shift_y_submaps = sp.y(),
        } );
        if( !gpu_residency_shifted ) {
            debugmsg( "SDL_GPU resident lighting input shift failed; see debug.log for details" );
            auto shifted_levels = std::vector<int> {};
            for( const auto gridz : std::views::iota( -OVERMAP_DEPTH, OVERMAP_HEIGHT + 1 ) ) {
                shifted_levels.push_back( gridz );
            }
            cata_gpu::invalidate_lighting_transparency_levels( shifted_levels );
        }
    }
#endif
    for( const auto gridz : std::views::iota( -OVERMAP_DEPTH, OVERMAP_HEIGHT + 1 ) ) {
        for( auto *veh : get_cache( gridz ).vehicle_list ) {
            veh->zones_dirty = true;
        }
    }

    const half_open_rectangle<point_bub_ms> boundaries_2d( point_bub_ms::zero(),
            point_bub_ms( g_mapsize_x, g_mapsize_y ) );
    const point_rel_ms shift_offset_pt( -sp.x() * SEEX, -sp.y() * SEEY );

    auto const shift_flat_cache = [&]( auto & cache, auto & scratch, level_cache & gc,
    const auto fill_value ) {
        using cache_value = std::ranges::range_value_t<std::remove_reference_t<decltype( cache )>>;
        scratch.assign( cache.begin(), cache.end() );

        const auto source_offset_x = sp.x() * SEEX;
        const auto source_offset_y = sp.y() * SEEY;
        const auto fill = static_cast<cache_value>( fill_value );
        const auto row_size = gc.cache_y;
        const auto valid_y_count = std::max( 0, row_size - std::abs( source_offset_y ) );
        const auto dst_y_begin = std::max( 0, -source_offset_y );
        const auto source_y_begin = dst_y_begin + source_offset_y;

        auto const fill_row = [&]( const auto x ) {
            std::fill_n( cache.begin() + gc.idx( x, 0 ), row_size, fill );
        };

        if( valid_y_count <= 0 ) {
            for( const auto x : std::views::iota( 0, gc.cache_x ) ) {
                fill_row( x );
            }
            return;
        }

        for( const auto x : std::views::iota( 0, gc.cache_x ) ) {
            const auto source_x = x + source_offset_x;
            if( source_x < 0 || source_x >= gc.cache_x ) {
                fill_row( x );
                continue;
            }

            if( dst_y_begin > 0 ) {
                std::fill_n( cache.begin() + gc.idx( x, 0 ), dst_y_begin, fill );
            }
            std::copy_n( scratch.begin() + gc.idx( source_x, source_y_begin ), valid_y_count,
                         cache.begin() + gc.idx( x, dst_y_begin ) );
            const auto dst_y_end = dst_y_begin + valid_y_count;
            if( dst_y_end < row_size ) {
                std::fill_n( cache.begin() + gc.idx( x, dst_y_end ), row_size - dst_y_end, fill );
            }
        }
    };
    auto float_shift_scratch = std::vector<float> {};
    auto char_shift_scratch = std::vector<char> {};
    auto short_shift_scratch = std::vector<short> {};
    auto bool_shift_scratch = std::vector<bool> {};

    auto const shift_submap_dirty_bits = [&]( cata_dynamic_bitset & dirty_bits, level_cache & gc ) {
        const auto old_dirty = dirty_bits;
        dirty_bits.reset();
        for( const auto smx : std::views::iota( 0, my_MAPSIZE ) ) {
            const auto source_smx = smx + sp.x();
            for( const auto smy : std::views::iota( 0, my_MAPSIZE ) ) {
                const auto source_smy = smy + sp.y();
                if( source_smx >= 0 && source_smx < my_MAPSIZE && source_smy >= 0 &&
                    source_smy < my_MAPSIZE &&
                    old_dirty.test( static_cast<size_t>( gc.bidx( source_smx, source_smy ) ) ) ) {
                    dirty_bits.set( static_cast<size_t>( gc.bidx( smx, smy ) ) );
                }
            }
        }
    };
    auto const mark_shifted_submap_bands = [&]( const int gridz, const int band_count, auto mark ) {
        auto const mark_column = [&]( const int smx ) {
            if( smx < 0 || smx >= my_MAPSIZE ) {
                return;
            }
            for( const auto smy : std::views::iota( 0, my_MAPSIZE ) ) {
                mark( tripoint_bub_sm( smx, smy, gridz ) );
            }
        };
        auto const mark_row = [&]( const int smy ) {
            if( smy < 0 || smy >= my_MAPSIZE ) {
                return;
            }
            for( const auto smx : std::views::iota( 0, my_MAPSIZE ) ) {
                mark( tripoint_bub_sm( smx, smy, gridz ) );
            }
        };

        for( const auto band : std::views::iota( 0, band_count ) ) {
            if( sp.x() > 0 ) {
                mark_column( my_MAPSIZE - 1 - band );
            } else if( sp.x() < 0 ) {
                mark_column( band );
            }
            if( sp.y() > 0 ) {
                mark_row( my_MAPSIZE - 1 - band );
            } else if( sp.y() < 0 ) {
                mark_row( band );
            }
        }
    };
    auto const mark_shifted_map_caches_dirty = [&]( auto const gridz ) {
        ZoneScopedN( "shift_mark_map_caches_dirty" );
        auto &gc = get_cache( gridz );
        auto const mark_submap_dirty = [&]( const tripoint_bub_sm & smp,
        const mapbuffer_mark_submap_caches_dirty_options & dirty_options ) {
            const auto abs_sm = map_local_to_abs( *this, smp );
            auto options = dirty_options;
            options.begin = abs_sm.xy();
            options.end = abs_sm.xy() + point_rel_sm( 1, 1 );
            options.zlev = abs_sm.z();
            get_mapbuffer().mark_submap_caches_dirty( options );
        };

        auto const mark_floor = [&]( const tripoint_bub_sm & smp ) {
            gc.floor_cache_dirty.set( static_cast<size_t>( gc.bidx( smp.x(), smp.y() ) ) );
            mark_submap_dirty( smp, { .floor = true } );
        };
        auto const mark_outside = [&]( const tripoint_bub_sm & smp ) {
            gc.outside_cache_dirty.set( static_cast<size_t>( gc.bidx( smp.x(), smp.y() ) ) );
            mark_submap_dirty( smp, { .outside = true } );
        };
        auto const mark_transparency = [&]( const tripoint_bub_sm & smp ) {
            gc.transparency_cache_dirty.set( static_cast<size_t>( gc.bidx( smp.x(), smp.y() ) ) );
            mark_submap_dirty( smp, { .transparency = true } );
        };

        mark_shifted_submap_bands( gridz, 1, mark_floor );
        mark_shifted_submap_bands( gridz, 3, mark_outside );
        mark_shifted_submap_bands( gridz, 3, mark_transparency );
    };
    auto const mark_shifted_absorption_cache_dirty = [&]( level_cache & gc, const int gridz ) {
        auto const mark = [&]( const tripoint_bub_sm & smp ) {
            if( smp.x() < 0 || smp.x() >= my_MAPSIZE || smp.y() < 0 || smp.y() >= my_MAPSIZE ) {
                return;
            }
            gc.absorption_cache_dirty.set( static_cast<size_t>( gc.bidx( smp.x(), smp.y() ) ) );
            const auto abs_sm = map_local_to_abs( *this, smp );
            get_mapbuffer().mark_submap_caches_dirty( {
                .begin = abs_sm.xy(),
                .end = abs_sm.xy() + point_rel_sm( 1, 1 ),
                .zlev = abs_sm.z(),
                .absorption = true,
            } );
        };
        mark_shifted_submap_bands( gridz, 2, mark );
    };

    // Run any Lua on_mapgen_postprocess hooks that were deferred from worker
    // threads (Lua is not thread-safe).  The submaps are already in the
    // mapbuffer; run_deferred_mapgen_hooks() binds them through mapgen constructors
    // and executes each hook on the main thread.
    {
        ZoneScopedN( "shift_mapgen_hooks" );
        run_deferred_mapgen_hooks();
        run_deferred_autonotes();
    }

    // Clear vehicle list and rebuild after shift
    {
        ZoneScopedN( "shift_clear_vehicle_cache" );
        clear_vehicle_cache( );
    }
    // Shift the map sx submaps to the right and sy submaps down.
    // sx and sy should never be bigger than +/-1.
    // absx and absy are our position in the world, for saving/loading purposes.
    {
        ZoneScopedN( "shift_grid_copy_load" );
        for( const auto gridz : std::views::iota( -OVERMAP_DEPTH, OVERMAP_HEIGHT + 1 ) ) {
            clear_vehicle_list( gridz );
            {
                ZoneScopedN( "shift_memory_seen_cache" );
                auto &gc = get_cache( gridz );
                shift_bitset_cache( gc.map_memory_seen_cache, gc.cache_x, SEEX, sp );
                gc.map_memory_seen_cache_dirty_points.clear();
                gc.map_memory_seen_cache_dirty_all = true;
            }
            {
                ZoneScopedN( "shift_prerequisite_caches" );
                auto &gc = get_cache( gridz );
                shift_flat_cache( gc.transparency_cache, float_shift_scratch, gc,
                                  LIGHT_TRANSPARENCY_OPEN_AIR );
                shift_flat_cache( gc.floor_cache, char_shift_scratch, gc, '\x01' );
                shift_flat_cache( gc.outside_cache, char_shift_scratch, gc, '\0' );
                shift_flat_cache( gc.sheltered_cache, char_shift_scratch, gc, '\x01' );
                shift_submap_dirty_bits( gc.transparency_cache_dirty, gc );
                shift_submap_dirty_bits( gc.floor_cache_dirty, gc );
                shift_submap_dirty_bits( gc.outside_cache_dirty, gc );
            }
            {
                ZoneScopedN( "shift_absorption_cache" );
                auto &gc = get_cache( gridz );
                shift_flat_cache( gc.absorption_cache, short_shift_scratch, gc,
                                  static_cast<short>( SOUND_ABSORPTION_OPEN_FIELD ) );
                shift_flat_cache( gc.sound_wall_cache, bool_shift_scratch, gc, false );
                shift_submap_dirty_bits( gc.absorption_cache_dirty, gc );
            }
            // Iterate in shift-direction order so copy_grid never reads an
            // already-overwritten source slot.  sp >= 0 → forward; sp < 0 → reverse.
            const auto for_grid_x = [&]( auto callback ) {
                if( sp.x() >= 0 ) {
                    std::ranges::for_each( std::views::iota( 0, my_MAPSIZE ), callback );
                } else {
                    std::ranges::for_each(
                        std::views::iota( 0, my_MAPSIZE ) | std::views::reverse, callback );
                }
            };
            const auto for_grid_y = [&]( auto callback ) {
                if( sp.y() >= 0 ) {
                    std::ranges::for_each( std::views::iota( 0, my_MAPSIZE ), callback );
                } else {
                    std::ranges::for_each(
                        std::views::iota( 0, my_MAPSIZE ) | std::views::reverse, callback );
                }
            };
            {
                ZoneScopedN( "shift_grid_slots" );
                ZoneValue( static_cast<uint64_t>( gridz + OVERMAP_DEPTH ) );
                for_grid_x( [&]( int gridx ) {
                    for_grid_y( [&]( int gridy ) {
                        if( gridx + sp.x() >= 0 && gridx + sp.x() < my_MAPSIZE &&
                            gridy + sp.y() >= 0 && gridy + sp.y() < my_MAPSIZE ) {
                            const auto grid_pos = tripoint_bub_sm{ gridx, gridy, gridz };
                            update_vehicle_list(
                                get_mapbuffer().lookup_submap_in_memory( map_local_to_abs( *this, grid_pos ) ),
                                gridz );
                        } else {
                            const auto grid_pos = tripoint_bub_sm( gridx, gridy, gridz );
                            loadn( grid_pos, true, true );
                            MAPBUFFER_REGISTRY.get( bound_dimension_ ).actualize_submap(
                                map_local_to_abs( *this, grid_pos ) );
                        }
                    } );
                } );
            }
            mark_shifted_map_caches_dirty( gridz );
            set_seen_cache_dirty( gridz );
            mark_visibility_cache_dirty( gridz );
            set_pathfinding_cache_dirty( gridz );
            set_suspension_cache_dirty( gridz );
            mark_shifted_absorption_cache_dirty( get_cache( gridz ), gridz );
        }
    } // shift_grid_copy_load

    {
        ZoneScopedN( "shift_reset_vehicle_cache" );
        reset_vehicle_cache( );
    }

    g->setremoteveh( remoteveh );

    if( !support_cache_dirty.empty() ) {
        shift_tripoint_set( support_cache_dirty, shift_offset_pt, boundaries_2d );
    }
    sounds::shift_sound_positions( shift_offset_pt );

    refresh_active_submap_view();
    validate_active_submap_view_complete( "map::shift" );
    invalidate_lightmap_caches();
}

auto map::apply_boundary_overlay( submap &sm, const tripoint_abs_sm &pos ) -> void
{
    const auto &pocket_info = get_mapbuffer().get_pocket_info();
    if( !pocket_info ) {
        return;
    }
    const bool on_min_x = pos.x() == pocket_info->bounds.min_bound.x();
    const bool on_max_x = pos.x() == pocket_info->bounds.max_bound.x();
    const bool on_min_y = pos.y() == pocket_info->bounds.min_bound.y();
    const bool on_max_y = pos.y() == pocket_info->bounds.max_bound.y();
    if( !on_min_x && !on_max_x && !on_min_y && !on_max_y ) {
        return;
    }
    const auto border = get_mapbuffer().get_boundary_terrain();
    std::ranges::for_each(
        cata::views::cartesian_product( std::views::iota( 0, SEEX ), std::views::iota( 0, SEEY ) )
    | std::views::filter( [&]( const auto & tile ) {
        const auto [x, y] = tile;
        return ( on_min_y && y == 0 ) ||
               ( on_max_y && y == SEEY - 1 ) ||
               ( on_min_x && x == 0 ) ||
               ( on_max_x && x == SEEX - 1 );
    } ),
    [&]( const auto & tile ) {
        const auto [x, y] = tile;
        sm.set_ter( { x, y }, border );
    }
    );
}

void map::loadn( const tripoint_bub_sm &grid, const bool update_vehicles,
                 const bool incremental )
{
    ZoneScopedN( "map_loadn" );
    ZoneValue( static_cast<uint64_t>( grid.z() + OVERMAP_DEPTH ) );

    const auto grid_abs_sub = map_local_to_abs( *this, tripoint_bub_sm( grid ) );
    auto &dim_buf = MAPBUFFER_REGISTRY.get( bound_dimension_ );

    // For out-of-bounds areas in bounded dimensions, use uniform boundary terrain
    // submaps instead of nullptr.  We check in-memory only (no DB lookup) because
    // most pocket-dimension submaps are out-of-bounds.
    if( get_mapbuffer().is_outside_pocket_dimension_bounds( tripoint_abs_sm( grid_abs_sub ) ) ) {
        mapbuffer &dim_buf = get_mapbuffer();
        submap *bsub = dim_buf.lookup_submap_in_memory( grid_abs_sub );
        // Diagnostic: log boundary submap creation for dimension debugging
        if( bsub == nullptr ) {
            add_msg( m_debug, "[DIM-DIAG] loadn: creating boundary submap at (%d,%d,%d)",
                     grid_abs_sub.x(), grid_abs_sub.y(), grid_abs_sub.z() );
            auto sm = std::make_unique<submap>( grid_abs_sub, bound_dimension_ );
            sm->is_uniform = true;
            sm->set_all_ter( get_mapbuffer().get_boundary_terrain() );
            sm->last_touched = calendar::turn;
            dim_buf.add_submap( grid_abs_sub, sm );
            bsub = dim_buf.lookup_submap_in_memory( grid_abs_sub );
        }
        return;
    }

    // Use the dimension-specific mapbuffer slot so each dimension's submaps
    // live independently and on_submap_loaded() finds them in the correct registry.
    auto *tmpsub = static_cast<submap *>( nullptr );
    {
        ZoneScopedN( "loadn_lookup" );
        tmpsub = get_mapbuffer().lookup_submap( grid_abs_sub );
    }
    // Diagnostic: log in-bounds submap loading for dimension transition debugging
    if( get_mapbuffer().has_dimension_bounds() ) {
        add_msg( m_debug, "[DIM-DIAG] loadn: in-bounds submap at (%d,%d,%d) %s",
                 grid_abs_sub.x(), grid_abs_sub.y(), grid_abs_sub.z(),
                 tmpsub ? "found" : "MISSING - will generate" );
    }
    if( tmpsub == nullptr ) {
        ZoneScopedN( "loadn_generate" );
        // It doesn't exist; we must generate it!
        dbg( DL::Info ) << "map::loadn: Missing mapbuffer data.  Regenerating.";

        // Each overmap square is two nonants; to prevent overlap, generate only at
        //  squares divisible by 2.
        // TODO: fix point types
        const auto grid_abs_omt = tripoint_abs_omt( project_to<coords::omt>( grid_abs_sub ) );
        auto &dim_buf = get_mapbuffer();
        dim_buf.generate_omt( grid_abs_omt );

        {
            ZoneScopedN( "loadn_generate_lookup" );
            tmpsub = get_mapbuffer().lookup_submap( grid_abs_sub );
        }
        if( tmpsub == nullptr ) {
            debugmsg( "failed to generate a submap at %s", grid_abs_sub.to_string() );
            return;
        }
    }

    // New submap changes the content of the map and all caches must be recalculated.
    // In incremental mode (shift context), transparency and floor caches were
    // shifted in shift() — only mark this specific submap dirty for those two.
    // In shift context, map::shift marks the whole z-level dirty once after
    // all grid slots are moved.  Avoid repeating that full-level work here for
    // every newly loaded edge submap.
    {
        ZoneScopedN( "loadn_dirty" );
        if( incremental ) {
            auto &ch = get_cache( grid.z() );
            ch.transparency_cache_dirty.set( static_cast<size_t>( ch.bidx( grid.x(), grid.y() ) ) );
            ch.floor_cache_dirty.set( static_cast<size_t>( ch.bidx( grid.x(), grid.y() ) ) );
            get_mapbuffer().mark_submap_caches_dirty( {
                .begin = grid_abs_sub.xy(),
                .end = grid_abs_sub.xy() + point_rel_sm( 1, 1 ),
                .zlev = grid_abs_sub.z(),
                .transparency = true,
                .floor = true,
                .outside = true,
                .absorption = true,
                .pathfinding = true,
            } );
        } else {
            set_transparency_cache_dirty( grid.z() );
            set_floor_cache_dirty( grid.z() );
            set_outside_cache_dirty( grid.z() );
            set_absorption_cache_dirty( grid.z() );
            set_seen_cache_dirty( grid.z() );
            set_pathfinding_cache_dirty( grid.z() );
            set_suspension_cache_dirty( grid.z() );
        }
    }
    // Overlay boundary terrain on the edge tiles of this submap if it sits at the
    // edge of a bounded dimension.  Must run before reality-bubble actualization
    // so actualize() sees the correct terrain.  The saved submap data is not modified.
    if( get_mapbuffer().has_dimension_bounds() ) {
        apply_boundary_overlay( *tmpsub, tripoint_abs_sm( grid_abs_sub ) );
    }
    get_mapbuffer().refresh_active_item_submap_index( tripoint_abs_sm( grid_abs_sub ),
            resident_item_lookup() );
    // field_cache removed — field_count is queried directly on each submap
    // Destroy bugged no-part vehicles
    {
        ZoneScopedN( "loadn_vehicles" );
        auto &veh_vec = tmpsub->vehicles;
        for( auto iter = veh_vec.begin(); iter != veh_vec.end(); ) {
            auto *veh = iter->get();
            if( veh->part_count() > 0 ) {
                // Always fix submap coordinates for easier Z-level-related operations
                veh->abs_sm_pos = grid_abs_sub;
                veh->set_dimension( bound_dimension_ );
                veh->attach();
                iter++;
            } else {
                reset_vehicle_cache();
                get_mapbuffer().unregister_vehicle( veh );
                if( veh->tracking_on ) {
                    get_overmapbuffer( bound_dimension_ ).remove_vehicle( veh );
                }
                dirty_vehicle_list.erase( veh );
                iter = veh_vec.erase( iter );
            }
        }

        // Update vehicle data
        if( update_vehicles ) {
            auto &map_cache = get_cache( grid.z() );
            for( const auto &veh : tmpsub->vehicles ) {
                // Only add if not tracking already.
                if( !map_cache.vehicle_list.contains( veh.get() ) ) {
                    map_cache.vehicle_list.insert( veh.get() );
                    if( !veh->loot_zones.empty() ) {
                        map_cache.zone_vehicles.insert( veh.get() );
                    }
                    add_vehicle_to_cache( veh.get() );
                }
            }
        }
    }
}

void map::spawn_monsters_submap_group( const tripoint_bub_sm &gp, mongroup &group,
                                       bool ignore_sight )
{
    const int s_range = std::min( g_half_mapsize_x,
                                  g->u.sight_range( g->light_level( g->u.bub_pos().z() ) ) );
    int pop = group.population;
    std::vector<tripoint_bub_ms> locations;
    if( !ignore_sight ) {
        // If the submap is one of the outermost submaps, assume that monsters are
        // invisible there.
        if( gp.x() == 0 || gp.y() == 0 || gp.x() + 1 == my_MAPSIZE || gp.y() + 1 == my_MAPSIZE ) {
            ignore_sight = true;
        }
    }

    if( gp.z() != g->u.bub_pos().z() ) {
        // Note: this is only OK because 3D vision isn't a thing yet
        ignore_sight = true;
    }

    static const auto allow_on_terrain = [&]( const tripoint_bub_ms & p ) {
        // TODO: flying creatures should be allowed to spawn without a floor,
        // but the new creature is created *after* determining the terrain, so
        // we can't check for it here.
        return passable( p ) && has_floor( p );
    };

    // If the submap is uniform, we can skip many checks
    const auto current_submap = active_submaps_.get_submap_view( map_local_to_abs( *this, gp ) );
    if( !current_submap ) {
        return;
    }
    bool ignore_terrain_checks = false;
    bool ignore_inside_checks = false;
    if( current_submap->get_submap().is_uniform ) {
        const tripoint_bub_ms upper_left{ SEEX * gp.x(), SEEY * gp.y(), gp.z() };
        if( !allow_on_terrain( upper_left ) ||
            ( !ignore_inside_checks && !is_outside( upper_left ) ) ) {
            const auto glp = map_local_to_abs( *this, gp );
            dbg( DL::Warn ) << "Empty locations for group " << group.type.str()
                            << " at uniform submap " << gp << " global " << glp;
            return;
        }

        ignore_terrain_checks = true;
        ignore_inside_checks = true;
    }

    for( const auto sm_ms : submap_tiles() ) {
        const auto fp = project_combine( gp, sm_ms );
        // If there is already a creature at this location, skip it
        if( ( g->critter_at( fp ) == nullptr ) &&
            // Skip impassable terrain
            ( ignore_terrain_checks || allow_on_terrain( fp ) ) &&
            // monster must spawn outside the viewing range of the player
            ( ignore_sight || !sees( g->u.bub_pos(), fp, s_range ) ) &&
            // monster must spawn outside.
            ( ignore_inside_checks || is_outside( fp ) ) &&
            // hordes must not appear inside player-owned vehicles.
            ( !horde_should_avoid_vehicle_tile( *this, fp, group ) ) ) {
            locations.push_back( fp );
        }
    }

    if( locations.empty() ) {
        // TODO: what now? there is no possible place to spawn monsters, most
        // likely because the player can see all the places.
        const auto glp = map_local_to_abs( *this, gp );
        dbg( DL::Warn ) << "Empty locations for group " << group.type.str()
                        << " at " << gp << " global " << glp;
        // Just kill the group. It's not like we're removing existing monsters
        // Unless it's a horde - then don't kill it and let it spawn behind a tree or smoke cloud
        if( !group.horde ) {
            group.clear();
        }

        return;
    }

    if( pop ) {
        // Populate the group from its population variable.
        for( int m = 0; m < pop; m++ ) {
            MonsterGroupResult spawn_details = MonsterGroupManager::GetResultFromGroup( group.type, &pop );
            if( !spawn_details.name ) {
                continue;
            }
            monster tmp( spawn_details.name );

            // If a monster came from a horde population, configure them to always be willing to rejoin a horde.
            if( group.horde ) {
                tmp.set_horde_attraction( MHA_ALWAYS );
            }
            for( int i = 0; i < spawn_details.pack_size; i++ ) {
                group.monsters.push_back( tmp );
            }
        }
    }

    // Find horde's target submap
    // TODO: fix point types
    auto horde_target = project_to<coords::ms>( group.target );
    for( auto &tmp : group.monsters ) {
        for( int tries = 0; tries < 10 && !locations.empty(); tries++ ) {
            const auto p = random_entry_removed( locations );
            if( !tmp.can_move_to( p ) ) {
                continue; // target can not contain the monster
            }
            if( group.horde ) {
                // Give monster a random point near horde's expected destination
                const auto rand_dest = abs_to_map_local( *this, horde_target ) +
                                       point_rel_ms( rng( 0, SEEX ), rng( 0, SEEY ) );
                const int turns = rl_dist( p, rand_dest ) + group.interest;
                tmp.wander_to( rand_dest, turns );
                add_msg( m_debug, "%s targeting %d,%d,%d", tmp.disp_name(),
                         tmp.wander_pos.x(), tmp.wander_pos.y(), tmp.wander_pos.z() );
            }

            monster *const placed = g->place_critter_at( make_shared_fast<monster>( tmp ), p );
            if( placed ) {
                placed->on_load();
            }
            break;
        }
    }
    // indicates the group is empty, and can be removed later
    group.clear();
}

void map::spawn_monsters_submap( const tripoint_bub_sm &gp, bool ignore_sight )
{
    // Load unloaded monsters
    // TODO: fix point types
    get_overmapbuffer( bound_dimension_ ).spawn_monster( map_local_to_abs( *this, gp ) );

    // Only spawn new monsters after existing monsters are loaded.
    // TODO: fix point types
    auto groups = get_overmapbuffer( bound_dimension_ ).groups_at( map_local_to_abs( *this, gp ) );
    for( auto &mgp : groups ) {
        spawn_monsters_submap_group( gp, *mgp, ignore_sight );
    }

    submap *const current_submap = get_mapbuffer().lookup_submap_in_memory(
                                       map_local_to_abs( *this, gp ) );
    if( current_submap == nullptr ) {
        return;
    }
    const auto gp_ms = project_to<coords::ms>( gp );

    for( auto &i : current_submap->spawns ) {
        const auto center = gp_ms + i.pos.raw();
        const tripoint_range<tripoint_bub_ms> points = points_in_radius( center, 3 );

        for( int j = 0; j < i.count; j++ ) {
            monster tmp( i.type );
            tmp.mission_id = i.mission_id;
            if( i.mission_id != -1 ) {
                mission *found_mission = mission::find( i.mission_id );
                if( found_mission != nullptr &&
                    found_mission->get_type().goal == MGOAL_KILL_MONSTERS ) {
                    found_mission->register_kill_needed();
                }
            }
            if( i.name != "NONE" ) {
                tmp.unique_name = i.name;
            }
            if( i.is_friendly() ) {
                tmp.friendly = -1;
            }

            const auto valid_location = [&]( const tripoint_bub_ms & p ) {
                // Checking for creatures via g is only meaningful if this is the main game map.
                // If it's some local map instance, the coordinates will most likely not even match.
                return ( !g || &get_map() != this || !g->critter_at( p ) ) && tmp.can_move_to( p );
            };

            const auto place_it = [&]( const tripoint_bub_ms & p ) {
                monster *const placed = g->place_critter_at( make_shared_fast<monster>( tmp ), p );
                if( placed ) {
                    placed->on_load();
                    cata::run_hooks( "on_creature_spawn", [&]( sol::table & params ) {
                        params["creature"] = placed;
                    } );
                    cata::run_hooks( "on_monster_spawn", [&]( sol::table & params ) {
                        params["monster"] = placed;
                    } );
                    if( i.disposition == spawn_disposition::SpawnDisp_Pet ) {
                        placed->make_pet();
                    }
                }
            };

            // First check out defined spawn location for a valid placement, and if that doesn't work
            // then fall back to picking a random point that is a valid location.
            if( valid_location( center ) ) {
                place_it( center );
            } else if( const std::optional<tripoint_bub_ms> pos = random_point( points, valid_location ) ) {
                place_it( *pos );
            }
        }
    }
    current_submap->spawns.clear();
}

void map::spawn_monsters( bool ignore_sight )
{
    ZoneScoped;
    for( const auto gp : bubble_submaps() ) {
        spawn_monsters_submap( gp, ignore_sight );
    }
}

auto map::spawn_monsters_new_submaps( const point_rel_sm &shift_amount ) -> void
{
    ZoneScoped;

    // If the shift covers the full map in any dimension every submap is new -
    // fall back to the full spawn to avoid an empty or degenerate strip.
    if( std::abs( shift_amount.x() ) >= my_MAPSIZE ||
        std::abs( shift_amount.y() ) >= my_MAPSIZE ) {
        spawn_monsters( false );
        return;
    }
    for( const auto gp : bubble_submaps() ) {
        if( inbounds( gp + shift_amount ) ) {
            spawn_monsters_submap( gp, false );
        }
    }
}

void map::clear_spawns()
{
    get_mapbuffer().clear_spawns( {
        .begin = abs_sub,
        .end = abs_sub + point_rel_sm( my_MAPSIZE, my_MAPSIZE ),
    } );
}

void map::clear_traps()
{
    get_mapbuffer().clear_traps( {
        .begin = abs_sub,
        .end = abs_sub + point_rel_sm( my_MAPSIZE, my_MAPSIZE ),
    } );
}

bool map::inbounds( const tripoint_bub_sm &p ) const
{
    // Use runtime my_MAPSIZE so this agrees with local submap-cache indexing.
    // MAPSIZE (and MAPSIZE_X/Y) are compile-time constants sized for the maximum
    // reality bubble; using them here when the live map is smaller allows p values
    // that pass inbounds() but produce an out-of-bounds cache index.
    const auto max_xy = my_MAPSIZE;
    return inbounds_z( p.z() ) && p.x() >= 0 && p.x() < max_xy && p.y() >= 0 && p.y() < max_xy;
}

bool map::inbounds( const tripoint_abs_sm &p ) const
{
    return inbounds( abs_to_map_local( *this, p ) );
}

bool map::inbounds( const point_bub_sm &p ) const
{
    const auto max_xy = my_MAPSIZE;
    return p.x() >= 0 && p.x() < max_xy && p.y() >= 0 && p.y() < max_xy;
}

bool map::is_position_simulated( const tripoint_bub_sm &p ) const
{
    return submap_loader.is_simulated( bound_dimension_, map_local_to_abs( *this, p ) );
}

void map::set_graffiti( const tripoint_bub_ms &p, const std::string &contents )
{
    get_mapbuffer().set_graffiti( map_local_to_abs( *this, p ), contents, resident_item_lookup() );
}

void map::delete_graffiti( const tripoint_bub_ms &p )
{
    get_mapbuffer().delete_graffiti( map_local_to_abs( *this, p ), resident_item_lookup() );
}

const std::string &map::graffiti_at( const tripoint_bub_ms &p ) const
{
    point_sm_ms l;
    submap *const current_submap = get_submap_at( tripoint_bub_ms( p ), l );
    if( current_submap == nullptr ) {
        static const std::string empty_string;
        return empty_string;
    }
    return current_submap->get_graffiti( l );
}

bool map::has_graffiti_at( const tripoint_bub_ms &p ) const
{
    return get_mapbuffer().has_graffiti_at( map_local_to_abs( *this, p ), resident_item_lookup() );
}

int map::determine_wall_corner( const tripoint_bub_ms &p ) const
{
    int test_connect_group = ter( p ).obj().connect_group;
    uint8_t connections = get_known_connections( p, test_connect_group );
    // The bits in connections are SEWN, whereas the characters in LINE_
    // constants are NESW, so we want values in 8 | 2 | 1 | 4 order.
    switch( connections ) {
        case 8 | 2 | 1 | 4:
            return LINE_XXXX;
        case 0 | 2 | 1 | 4:
            return LINE_OXXX;

        case 8 | 0 | 1 | 4:
            return LINE_XOXX;
        case 0 | 0 | 1 | 4:
            return LINE_OOXX;

        case 8 | 2 | 0 | 4:
            return LINE_XXOX;
        case 0 | 2 | 0 | 4:
            return LINE_OXOX;
        case 8 | 0 | 0 | 4:
            return LINE_XOOX;
        case 0 | 0 | 0 | 4:
            return LINE_OXOX; // LINE_OOOX would be better

        case 8 | 2 | 1 | 0:
            return LINE_XXXO;
        case 0 | 2 | 1 | 0:
            return LINE_OXXO;
        case 8 | 0 | 1 | 0:
            return LINE_XOXO;
        case 0 | 0 | 1 | 0:
            return LINE_XOXO; // LINE_OOXO would be better
        case 8 | 2 | 0 | 0:
            return LINE_XXOO;
        case 0 | 2 | 0 | 0:
            return LINE_OXOX; // LINE_OXOO would be better
        case 8 | 0 | 0 | 0:
            return LINE_XOXO; // LINE_XOOO would be better

        case 0 | 0 | 0 | 0:
            return ter( p ).obj().symbol(); // technically just a column

        default:
            // assert( false );
            // this shall not happen
            return '?';
    }
}

void map::build_outside_cache( const int zlev )
{
    ZoneScopedN( "build_outside_cache" );
    auto &ch = get_cache( zlev );
    if( ch.outside_cache_dirty.none() ) {
        return;
    }

    if( zlev >= OVERMAP_HEIGHT ) {
        // Base case: open sky at the top — every tile is outside, nothing above.
        std::fill( ch.outside_cache.begin(), ch.outside_cache.end(), true );
        std::fill( ch.sheltered_cache.begin(), ch.sheltered_cache.end(), false );
        for( const auto p : flat_bubble_submaps() ) {
            const auto sm_pos = tripoint_bub_sm( p, zlev );
            auto *sm = get_mapbuffer().lookup_submap_in_memory( map_local_to_abs( *this, sm_pos ) );
            if( sm ) {
                std::ranges::fill( std::span( &sm->outside_cache[0][0], SEEX * SEEY ), true );
                sm->outside_dirty = false;
            }
        }
        ch.outside_cache_dirty.reset();
        return;
    }

    // Ensure z+1 floor and outside caches are current — they are the inputs.
    const int above_z = zlev + 1;
    if( inbounds_z( above_z ) ) {
        build_floor_cache( above_z );
        build_outside_cache( above_z );
    }

    const level_cache *above = inbounds_z( above_z ) ? &get_cache_ref( above_z ) : nullptr;
    const bool rebuild_all = ch.outside_cache_dirty.all();

    // Delegate to per-submap rebuild, then copy into the flat render cache.
    // Each smx column writes to unique flat positions; rebuild_outside_cache reads
    // only from the immutable above cache, so columns are safe to process concurrently.
    const auto process_smx = [&]( int smx ) {
        for( int smy = 0; smy < my_MAPSIZE; ++smy ) {
            if( !rebuild_all && !ch.outside_cache_dirty.test(
                    static_cast<size_t>( ch.bidx( smx, smy ) ) ) ) {
                continue;
            }
            const auto sm_pos = tripoint_bub_sm( smx, smy, zlev );
            auto *cur_submap = get_mapbuffer().lookup_submap_in_memory(
                                   map_local_to_abs( *this, sm_pos ) );
            if( cur_submap == nullptr ) {
                continue;
            }
            cur_submap->rebuild_outside_cache( above, sm_pos );

            for( const auto sm_ms : submap_tiles() ) {
                const auto ms_pos = project_combine( sm_pos, sm_ms );
                ch.outside_cache[static_cast<size_t>( ch.idx( ms_pos.x(), ms_pos.y() ) )] =
                    cur_submap->outside_cache[sm_ms.x()][sm_ms.y()];
            }
        }
    };

    if( parallel_enabled && parallel_map_cache && !is_pool_worker_thread() ) {
        parallel_for( 0, my_MAPSIZE, process_smx );
    } else {
        for( int smx = 0; smx < my_MAPSIZE; ++smx ) {
            process_smx( smx );
        }
    }

    ch.outside_cache_dirty.reset();
}

void map::build_obstacle_cache( const tripoint_bub_ms &start, const tripoint_bub_ms &end,
                                float *obstacle_cache, int cache_sy )
{
    const point_sm_ms min_submap{ std::max( 0, start.x() / SEEX ), std::max( 0, start.y() / SEEY ) };
    const point_sm_ms max_submap{
        std::min( my_MAPSIZE - 1, end.x() / SEEX ), std::min( my_MAPSIZE - 1, end.y() / SEEY ) };
    // Find and cache all the map obstacles.
    // For now setting obstacles to be extremely dense and fill their squares.
    // In future, scale effective obstacle density by the thickness of the obstacle.
    // Also consider modelling partial obstacles.
    // TODO: Support z-levels.
    for( int smx = min_submap.x(); smx <= max_submap.x(); ++smx ) {
        for( int smy = min_submap.y(); smy <= max_submap.y(); ++smy ) {
            const auto gridp = tripoint_bub_sm( smx, smy, start.z() );
            const auto view = active_submaps_.get_submap_view( point_rel_sm( gridp.x(), gridp.y() ),
                              gridp.z() );
            if( !view ) {
                for( const auto sm_ms : submap_tiles() ) {
                    const auto ms_pos = project_combine( gridp, sm_ms );
                    obstacle_cache[ms_pos.x() * cache_sy + ms_pos.y()] = 1000.0f;
                }
                continue;
            }
            const submap &cur_submap = view->get_submap();

            // TODO: Init indices to prevent iterating over unused submap sections.
            for( const auto sm_ms : submap_tiles() ) {
                int ter_move = cur_submap.get_ter( sm_ms ).obj().movecost;
                int furn_move = cur_submap.get_furn( sm_ms ).obj().movecost;
                const auto ms_pos = project_combine( gridp, sm_ms );
                if( ter_move == 0 || furn_move < 0 || ter_move + furn_move == 0 ) {
                    obstacle_cache[ms_pos.x() * cache_sy + ms_pos.y()] = 1000.0f;
                } else {
                    obstacle_cache[ms_pos.x() * cache_sy + ms_pos.y()] = 0.0f;
                }
            }
        }
    }
    const auto start_sm = project_to<coords::sm>( start );
    const auto end_sm = project_to<coords::sm>( end );
    VehicleList vehs = get_vehicles( start_sm, end_sm );
    const inclusive_cuboid<tripoint_bub_ms> bounds( start, end );
    // Cache all the vehicle stuff in one loop
    for( auto &v : vehs ) {
        for( const vpart_reference &vp : v.v->get_all_parts() ) {
            auto p = v.pos + vp.part().precalc[0];
            if( p.z() != start.z() ) {
                break;
            }
            if( !bounds.contains( p ) ) {
                continue;
            }

            if( vp.obstacle_at_part() ) {
                obstacle_cache[p.x() * cache_sy + p.y()] = 1000.0f;
            }
        }
    }

}

bool map::build_floor_cache( const int zlev )
{
    ZoneScopedN( "build_floor_cache" );
    auto &ch = get_cache( zlev );
    if( ch.floor_cache_dirty.none() ) {
        return false;
    }

    auto &floor_cache = ch.floor_cache;
    const bool rebuild_all = ch.floor_cache_dirty.all();

    // When rebuilding all submaps we can bulk-initialize the whole level to
    // "has floor" (true) in one pass, then let per-submap rebuilds stamp out
    // the no-floor tiles.  For partial rebuilds we reset only dirty submap
    // regions individually inside the loop.
    if( rebuild_all ) {
        std::fill( floor_cache.begin(), floor_cache.end(), true );
    }

    // Delegate to per-submap rebuild, then copy into the flat render cache.
    for( const auto p : flat_bubble_submaps() ) {
        if( !rebuild_all && !ch.floor_cache_dirty.test(
                static_cast<size_t>( ch.bidx( p.x(), p.y() ) ) ) ) {
            continue;
        }
        const auto sm_pos = tripoint_bub_sm( p, zlev );
        submap *cur_submap = get_mapbuffer().lookup_submap_in_memory(
                                 map_local_to_abs( *this, sm_pos ) );
        if( cur_submap == nullptr ) {
            // Null expected for circle corners and bounded-dimension edges.
            continue;
        }
        cur_submap->rebuild_floor_cache( *this, sm_pos );

        const auto ms_pos = project_to<coords::ms>( p );

        if( !rebuild_all ) {
            // Reset this submap's region to "has floor" before stamping no-floor tiles,
            // since a previously no-floor tile may have gained a floor since last build.
            for( int sx = 0; sx < SEEX; ++sx ) {
                std::fill_n( floor_cache.data() + ch.idx( ms_pos.x() + sx, ms_pos.y() ),
                             SEEY, '\x01' );
            }
        }

        for( const auto sm_ms : submap_tiles() ) {
            if( !cur_submap->floor_cache[sm_ms.x()][sm_ms.y()] ) {
                floor_cache[ch.idx( ms_pos.x() + sm_ms.x(), ms_pos.y() + sm_ms.y() )] = false;
            }
        }
    }

    ch.floor_cache_dirty.reset();
    ch.has_any_floor = std::ranges::any_of( floor_cache,
    []( char c ) { return c != 0; } );
    return true;
}

void map::build_floor_caches()
{
    ZoneScoped;
    for( int z = -OVERMAP_DEPTH; z <= OVERMAP_HEIGHT; z++ ) {
        build_floor_cache( z );
    }
}

void map::update_suspension_cache( const int &z )
{
    level_cache &ch = get_cache( z );
    if( !ch.suspension_cache_dirty ) {
        return;
    }
    std::list<point_abs_ms> &suspension_cache = ch.suspension_cache;
    if( !ch.suspension_cache_initialized ) {
        for( const auto &view : active_submaps_.submaps( z ) ) {
            const submap &cur_submap = view.get_submap();
            const auto abs_sm = view.abs_pos().xy();
            for( const auto sm_ms : submap_tiles() ) {
                const ter_t &terrain = cur_submap.get_ter( sm_ms ).obj();
                if( terrain.has_flag( TFLAG_SUSPENDED ) ) {
                    suspension_cache.emplace_back( coords::project_combine( abs_sm, sm_ms ) );
                }
            }
        }
        ch.suspension_cache_initialized = true;
    }

    for( auto iter = suspension_cache.begin(); iter != suspension_cache.end(); ) {
        const point_abs_ms absp = *iter;
        const tripoint_bub_ms loctp( abs_to_map_local( *this, absp ), z );
        if( !inbounds( loctp ) ) {
            ++iter;
            continue;
        }
        const submap *cur_submap = get_submap_at( loctp );
        if( cur_submap == nullptr ) {
            debugmsg( "Tried to run suspension check at (%d,%d,%d) but the submap is not loaded", loctp.x(),
                      loctp.y(), loctp.z() );
            ++iter;
            continue;
        }
        const ter_t &terrain = ter( loctp ).obj();
        if( terrain.has_flag( TFLAG_SUSPENDED ) ) {
            if( !is_suspension_valid( loctp ) ) {
                support_dirty( loctp );
                iter = suspension_cache.erase( iter );
            } else {
                ++iter;
            }
        } else {
            iter = suspension_cache.erase( iter );
        }
    }
    ch.suspension_cache_dirty = false;
}

static void vehicle_caching_internal( level_cache &zch, const vpart_reference &vp, vehicle *v )
{
    auto &outside_cache = zch.outside_cache;
    auto &sheltered_cache = zch.sheltered_cache;
    auto &transparency_cache = zch.transparency_cache;
    auto &floor_cache = zch.floor_cache;
    auto &obscured_cache = zch.vehicle_obscured_cache;
    auto &obstructed_cache = zch.vehicle_obstructed_cache;

    const size_t part = vp.part_index();
    const tripoint_bub_ms &part_pos =  v->bub_part_location( vp.part() );

    bool vehicle_is_opaque = vp.has_feature( VPFLAG_OPAQUE ) && !vp.part().is_broken();

    if( vehicle_is_opaque ) {
        int dpart = v->part_with_feature( part, VPFLAG_OPENABLE, true );
        if( dpart < 0 || !v->part( dpart ).open ) {
            transparency_cache[zch.idx( part_pos.x(), part_pos.y() )] = LIGHT_TRANSPARENCY_SOLID;
        } else {
            vehicle_is_opaque = false;
        }
    }

    if( vehicle_is_opaque || vp.is_inside() ) {
        const int veh_idx = zch.idx( part_pos.x(), part_pos.y() );
        outside_cache[veh_idx] = false;
        sheltered_cache[veh_idx] = true;
    }

    if( vp.has_feature( VPFLAG_BOARDABLE ) && !vp.part().is_broken() ) {
        floor_cache[zch.idx( part_pos.x(), part_pos.y() )] = true;
    }

    tripoint_mnt_veh t = v->bubble_to_mount( part_pos + point_north_west );
    if( !v->allowed_light( t, vp.mount() ) ) {
        obscured_cache[zch.idx( part_pos.x(), part_pos.y() )].nw = true;
    }
    if( !v->allowed_move( t, vp.mount() ) ) {
        obstructed_cache[zch.idx( part_pos.x(), part_pos.y() )].nw = true;
    }

    t = v->bubble_to_mount( part_pos + point_north_east );
    if( !v->allowed_light( t, vp.mount() ) ) {
        obscured_cache[zch.idx( part_pos.x(), part_pos.y() )].ne = true;
    }
    if( !v->allowed_move( t, vp.mount() ) ) {
        obstructed_cache[zch.idx( part_pos.x(), part_pos.y() )].ne = true;
    }

    if( part_pos.x() > 0 && part_pos.y() < zch.cache_y - 1 ) {
        t = v->bubble_to_mount( part_pos + point_south_west );
        if( !v->allowed_light( t, vp.mount() ) ) {
            obscured_cache[zch.idx( part_pos.x() - 1, part_pos.y() + 1 )].ne = true;
        }
        if( !v->allowed_move( t, vp.mount() ) ) {
            obstructed_cache[zch.idx( part_pos.x() - 1, part_pos.y() + 1 )].ne = true;
        }
    }

    if( part_pos.x() < zch.cache_x - 1 && part_pos.y() < zch.cache_y - 1 ) {
        t = v->bubble_to_mount( tripoint_bub_ms( part_pos + point_south_east ) );
        if( !v->allowed_light( t, vp.mount() ) ) {
            obscured_cache[zch.idx( part_pos.x() + 1, part_pos.y() + 1 )].nw = true;
        }
        if( !v->allowed_move( t, vp.mount() ) ) {
            obstructed_cache[zch.idx( part_pos.x() + 1, part_pos.y() + 1 )].nw = true;
        }
    }
}

static void vehicle_caching_internal_above( level_cache &zch_above, const vpart_reference &vp,
        vehicle *v )
{
    if( vp.has_feature( VPFLAG_ROOF ) || vp.has_feature( VPFLAG_OPAQUE ) ) {
        const tripoint_bub_ms &part_pos = v->bub_part_location( vp.part() );
        const int tile_idx = zch_above.idx( part_pos.x(), part_pos.y() );
        zch_above.vehicle_floor_cache[tile_idx] = true;
    }
}

void map::do_vehicle_caching( int z )
{
    level_cache &ch = get_cache( z );
    for( vehicle *v : ch.vehicle_list ) {
        for( const vpart_reference &vp : v->get_all_parts() ) {
            const tripoint_bub_ms &part_pos = v->bub_part_location( vp.part() );
            if( !inbounds( part_pos ) || vp.part().removed ) {
                continue;
            }
            vehicle_caching_internal( get_cache( part_pos.z() ), vp, v );
            if( part_pos.z() < OVERMAP_HEIGHT ) {
                vehicle_caching_internal_above( get_cache( part_pos.z() + 1 ), vp, v );
            }
        }
    }
}

void map::build_map_cache( const int zlev, bool skip_lightmap )
{
    ZoneScoped;
#if defined( CATA_SDL )
    const auto use_sdl_gpu_compute = cata_compute::uses_sdl_gpu_compute();
#endif
    flush_lightmap_cpu_read_counters();
    const auto valid_lm_levels = std::ranges::count_if(
    std::views::iota( -OVERMAP_DEPTH, OVERMAP_HEIGHT + 1 ), [this]( const int z ) {
        return get_cache_ref( z ).lm_cpu_cache_valid;
    } );
    TracyPlot( "Map CPU LM Valid Levels", static_cast<int64_t>( valid_lm_levels ) );
    TracyPlot( "Map CPU LM Stale Levels",
               static_cast<int64_t>( OVERMAP_HEIGHT - -OVERMAP_DEPTH + 1 - valid_lm_levels ) );
    bool seen_cache_dirty = false;
    bool gpu_transparency_dirty = false;
    bool gpu_floor_dirty = false;
    bool gpu_vehicle_floor_dirty = false;
    bool gpu_vehicle_obscured_dirty = false;
    std::vector<int> dirty_seen_cache_levels;
    std::vector<int> gpu_transparency_dirty_levels;
    std::vector<int> gpu_transparency_residency_invalid_levels;
    std::vector<int> gpu_floor_dirty_levels;
    std::vector<int> gpu_vehicle_floor_dirty_levels;
    std::vector<int> gpu_vehicle_obscured_dirty_levels;

    auto mark_lightmap_dirty = [this]( const int z ) {
        auto &cache = get_cache( z );
        cache.lightmap_dirty = true;
        cache.lm_cpu_cache_valid = false;
        ++cache.lm_cpu_cache_generation;
        mark_visibility_cache_dirty( z );
    };
    auto add_gpu_dirty_level = []( auto & levels, const int z ) {
        if( z >= -OVERMAP_DEPTH && z <= OVERMAP_HEIGHT ) {
            levels.push_back( z );
        }
    };
    auto normalize_gpu_dirty_levels = []( auto & levels ) {
        std::ranges::sort( levels );
        levels.erase( std::ranges::unique( levels ).begin(), levels.end() );
    };
    auto level_has_vehicle_floor = []( const level_cache & ch ) {
        return std::ranges::any_of( ch.vehicle_floor_cache, []( const char c ) {
            return c != '\0';
        } );
    };

    // Refresh the shared weather-transparency lookup table once, serially,
    // before the parallel block.  build_transparency_cache() reads the
    // table on every call, so updating it here guarantees all workers see a
    // consistent value without a data race.
    update_weather_transparency_lookup();

    // Parallelize the expensive per-z-level cache builds across all z-levels.
    // Each build_*_cache(z) writes only to get_cache(z) — no z-level aliasing.
    //
    // update_suspension_cache is intentionally excluded from this parallel block:
    // it calls support_dirty() which inserts into the shared support_cache_dirty
    // set and is not thread-safe.  It runs in a dedicated serial pass below.

    {
        ZoneScopedN( "Phase1_floor" );
        // Floor caches are z-independent so they can run in any order.
        // They must complete before outside/sheltered caches which read floor[z+1].
        for( int z = -OVERMAP_DEPTH; z <= OVERMAP_HEIGHT; ++z ) {
            const bool floor_was_dirty = !get_cache_ref( z ).floor_cache_dirty.none();
            if( build_floor_cache( z ) ) {
                seen_cache_dirty = true;
            }
            if( floor_was_dirty ) {
                gpu_floor_dirty = true;
                add_gpu_dirty_level( gpu_floor_dirty_levels, z );
            }
        }
    }

    {
        ZoneScopedN( "Phase1_outside_sheltered" );
        // outside_cache and sheltered_cache both depend on floor[z+1] and their own z+1,
        // so they must be computed top-down.  They use intra-z parallel_for, so they
        // cannot run inside a parallel-over-z block.
        for( int z = OVERMAP_HEIGHT; z >= -OVERMAP_DEPTH; --z ) {
            build_outside_cache( z );
        }
    }

    {
        ZoneScopedN( "Phase1_transparency" );
        // Transparency depends on outside_cache; runs after outside is complete.
        auto const transparency_dirty_levels = build_transparency_caches( -OVERMAP_DEPTH, OVERMAP_HEIGHT );
        if( !transparency_dirty_levels.empty() ) {
            gpu_transparency_dirty = true;
            std::ranges::copy( transparency_dirty_levels,
                               std::back_inserter( gpu_transparency_dirty_levels ) );
            std::ranges::for_each( transparency_dirty_levels, mark_lightmap_dirty );
        }
    }

    {
        ZoneScopedN( "Phase1_parallel_caches" );
        // Vehicle cache clearing only — floor/outside/sheltered are already done above.
        if( parallel_enabled && parallel_map_cache ) {
            std::mutex dirty_mutex;
            parallel_for( -OVERMAP_DEPTH, OVERMAP_HEIGHT + 1, [&]( int z ) {
                level_cache &ch = get_cache( z );
                const bool vehicle_floor_was_dirty = level_has_vehicle_floor( ch );
                // vehicle_floor_cache is written by vehicles one level below (via
                // vehicle_caching_internal_above), so it must be cleared unconditionally —
                // not gated on veh_in_active_range — to prevent stale entries after shifts.
                std::fill( ch.vehicle_floor_cache.begin(), ch.vehicle_floor_cache.end(), '\0' );
                if( ch.veh_in_active_range ) {
                    const diagonal_blocks fill = {false, false};
                    std::fill( ch.vehicle_obscured_cache.begin(), ch.vehicle_obscured_cache.end(), fill );
                    std::fill( ch.vehicle_obstructed_cache.begin(), ch.vehicle_obstructed_cache.end(), fill );
                    std::lock_guard<std::mutex> lock( dirty_mutex );
                    add_gpu_dirty_level( gpu_vehicle_obscured_dirty_levels, z );
                }

                const bool level_seen_dirty = ch.seen_cache_dirty;
                if( level_seen_dirty || vehicle_floor_was_dirty ) {
                    std::lock_guard<std::mutex> lock( dirty_mutex );
                    if( level_seen_dirty ) {
                        seen_cache_dirty = true;
                        dirty_seen_cache_levels.push_back( z );
                    }
                    if( vehicle_floor_was_dirty ) {
                        add_gpu_dirty_level( gpu_vehicle_floor_dirty_levels, z );
                    }
                }
            } );
        } else {
            for( int z = -OVERMAP_DEPTH; z <= OVERMAP_HEIGHT; ++z ) {
                level_cache &ch = get_cache( z );
                const bool vehicle_floor_was_dirty = level_has_vehicle_floor( ch );

                // vehicle_floor_cache is written by vehicles one level below (via
                // vehicle_caching_internal_above), so it must be cleared unconditionally —
                // not gated on veh_in_active_range — to prevent stale entries after shifts.
                std::fill( ch.vehicle_floor_cache.begin(), ch.vehicle_floor_cache.end(), '\0' );
                if( ch.veh_in_active_range ) {
                    const diagonal_blocks fill = {false, false};
                    std::fill( ch.vehicle_obscured_cache.begin(), ch.vehicle_obscured_cache.end(), fill );
                    std::fill( ch.vehicle_obstructed_cache.begin(), ch.vehicle_obstructed_cache.end(), fill );
                    add_gpu_dirty_level( gpu_vehicle_obscured_dirty_levels, z );
                }

                const bool level_seen_dirty = ch.seen_cache_dirty;
                if( level_seen_dirty ) {
                    seen_cache_dirty = true;
                    dirty_seen_cache_levels.push_back( z );
                }
                if( vehicle_floor_was_dirty ) {
                    add_gpu_dirty_level( gpu_vehicle_floor_dirty_levels, z );
                }
            }
        }
    }
    // implicit barrier; floor/outside/sheltered/transparency caches for all z-levels are complete.

    {
        ZoneScopedN( "Phase2_suspension" );
        // update_suspension_cache calls support_dirty() which writes to the shared
        // support_cache_dirty set; must remain serial.
        for( int z = -OVERMAP_DEPTH; z <= OVERMAP_HEIGHT; z++ ) {
            update_suspension_cache( z );
        }
    }

    {
        ZoneScopedN( "Phase3_vehicles" );
        // needs a separate pass as it changes the caches on neighbour z-levels (e.g. floor_cache);
        // otherwise such changes might be overwritten by main cache-building logic.
        // This pass must remain serial: do_vehicle_caching() writes to neighbor z-level caches.
        auto const mark_vehicle_gpu_structural_levels = [&]( const vehicle * const veh ) {
            if( veh == nullptr ) {
                return;
            }
            for( const vpart_reference &vp : veh->get_all_parts() ) {
                const auto &part_pos = veh->bub_part_location( vp.part() );
                if( !inbounds( part_pos ) || vp.part().removed ) {
                    continue;
                }
                add_gpu_dirty_level( gpu_transparency_dirty_levels, part_pos.z() );
                add_gpu_dirty_level( gpu_transparency_residency_invalid_levels, part_pos.z() );
                add_gpu_dirty_level( gpu_floor_dirty_levels, part_pos.z() );
                add_gpu_dirty_level( gpu_vehicle_obscured_dirty_levels, part_pos.z() );
            }
        };
        for( int z = -OVERMAP_DEPTH; z <= OVERMAP_HEIGHT; z++ ) {
            if( get_cache( z ).veh_in_active_range ) {
                for( const vehicle *const veh : get_cache( z ).vehicle_list ) {
                    mark_vehicle_gpu_structural_levels( veh );
                }
                do_vehicle_caching( z );
            }
        }
        std::ranges::for_each( std::views::iota( -OVERMAP_DEPTH, OVERMAP_HEIGHT + 1 ), [&]( const int z ) {
            if( level_has_vehicle_floor( get_cache_ref( z ) ) ) {
                add_gpu_dirty_level( gpu_vehicle_floor_dirty_levels, z );
            }
        } );
    }

    normalize_gpu_dirty_levels( gpu_transparency_dirty_levels );
    normalize_gpu_dirty_levels( gpu_transparency_residency_invalid_levels );
    normalize_gpu_dirty_levels( gpu_floor_dirty_levels );
    normalize_gpu_dirty_levels( gpu_vehicle_floor_dirty_levels );
    normalize_gpu_dirty_levels( gpu_vehicle_obscured_dirty_levels );
    gpu_transparency_dirty = !gpu_transparency_dirty_levels.empty();
    gpu_floor_dirty = !gpu_floor_dirty_levels.empty();
    gpu_vehicle_floor_dirty = !gpu_vehicle_floor_dirty_levels.empty();
    gpu_vehicle_obscured_dirty = !gpu_vehicle_obscured_dirty_levels.empty();
#if defined( CATA_SDL )
    if( use_sdl_gpu_compute && !gpu_transparency_residency_invalid_levels.empty() ) {
        cata_gpu::invalidate_lighting_transparency_levels( gpu_transparency_residency_invalid_levels );
    }
    if( skip_lightmap && use_sdl_gpu_compute && gpu_transparency_dirty ) {
        cata_gpu::invalidate_lighting_transparency_levels( gpu_transparency_dirty_levels );
    }
#endif
    TracyPlot( "Map GPU Transparency Dirty Levels",
               static_cast<int64_t>( gpu_transparency_dirty_levels.size() ) );
    TracyPlot( "Map GPU Floor Dirty Levels",
               static_cast<int64_t>( gpu_floor_dirty_levels.size() ) );
    TracyPlot( "Map GPU Vehicle Floor Dirty Levels",
               static_cast<int64_t>( gpu_vehicle_floor_dirty_levels.size() ) );
    TracyPlot( "Map GPU Vehicle Obscured Dirty Levels",
               static_cast<int64_t>( gpu_vehicle_obscured_dirty_levels.size() ) );
    TracyPlot( "Map GPU Transparency Invalidated Levels",
               static_cast<int64_t>( gpu_transparency_residency_invalid_levels.size() ) );

    const tripoint_bub_ms &p = g->u.bub_pos();
    auto force_seen_rebuild_for_gpu_residency = false;
#if defined( CATA_SDL )
    SDL_GPUDevice *const gpu_device = use_sdl_gpu_compute ? cata_gpu::get_device() : nullptr;
    if( !skip_lightmap && use_sdl_gpu_compute && gpu_device != nullptr ) {
        const auto &visibility_cache = get_cache_ref( zlev );
        if( !cata_gpu::resident_lighting_ready_for_visibility( {
        .device = gpu_device,
        .cache_x = visibility_cache.cache_x,
        .cache_y = visibility_cache.cache_y,
        .z_count = OVERMAP_LAYERS,
    } ) ) {
            force_seen_rebuild_for_gpu_residency = true;
            invalidate_lightmap_caches();
            mark_visibility_cache_dirty( zlev );
        }
    }
#endif
    auto dirty_lightmap_levels = std::vector<int> {};
    if( !skip_lightmap ) {
        ZoneScopedN( "Phase4_lightmap_prepare" );
        invalidate_lightmap_caches_if_light_state_changed();
        auto needs_lightmap_dispatch = [this]( const int z ) {
            return get_cache( z ).lightmap_dirty;
        };
        std::ranges::copy_if( std::views::iota( -OVERMAP_DEPTH, OVERMAP_HEIGHT + 1 ),
                              std::back_inserter( dirty_lightmap_levels ), needs_lightmap_dispatch );

        // Only include levels whose lightmap is actually stale this redraw.
        // lightmap_dirty is set by explicit cache invalidation or dynamic light
        // source changes, then cleared after generate_lightmap runs.
        std::ranges::sort( dirty_lightmap_levels );
        dirty_lightmap_levels.erase( std::ranges::unique( dirty_lightmap_levels ).begin(),
                                     dirty_lightmap_levels.end() );
        TracyPlot( "Map Dirty LM Levels", static_cast<int64_t>( dirty_lightmap_levels.size() ) );
    }

#if defined( CATA_SDL )
    auto pending_gpu_lighting = cata_gpu::gpu_lighting_work {};
    if( !skip_lightmap && use_sdl_gpu_compute && gpu_device != nullptr &&
        !dirty_lightmap_levels.empty() ) {
        ZoneScopedN( "Phase4_lightmap_begin" );
        update_solar_params();
        // GPU path: lightmap rebuilds only run for lightmap-dirty levels.
        // Player movement and other FoV-only updates are handled by
        // update_visibility_cache(), which can rebuild resident seen data.
        for( const int z : dirty_lightmap_levels ) {
            auto &c = get_cache( z );
            std::fill( c.sm.begin(), c.sm.end(), 0.0f );
            std::fill( c.light_source_buffer.begin(),
                       c.light_source_buffer.end(), 0.0f );
            std::fill( c.colored_light_source_buffer.begin(),
                       c.colored_light_source_buffer.end(), 0.0f );
            std::fill( c.light_source_color_buffer.begin(),
                       c.light_source_color_buffer.end(), 0u );
            c.light_source_points.clear();
            std::ranges::fill( c.lm, 0.0f );
            c.lm_cpu_cache_valid = false;
            ++c.lm_cpu_cache_generation;
        }
        pending_gpu_lighting = cata_gpu::begin_gpu_lighting( gpu_device, {
            .m            = this,
            .dirty_levels = &dirty_lightmap_levels,
            .seen_dirty_levels = &dirty_seen_cache_levels,
            .player_x     = p.x(),
            .player_y     = p.y(),
            .player_zlev  = zlev,
            .transparency_dirty = gpu_transparency_dirty,
            .transparency_dirty_levels = &gpu_transparency_dirty_levels,
            .floor_dirty = gpu_floor_dirty,
            .floor_dirty_levels = &gpu_floor_dirty_levels,
            .vehicle_floor_dirty = gpu_vehicle_floor_dirty,
            .vehicle_floor_dirty_levels = &gpu_vehicle_floor_dirty_levels,
            .vehicle_obscured_dirty = gpu_vehicle_obscured_dirty,
            .vehicle_obscured_dirty_levels = &gpu_vehicle_obscured_dirty_levels,
            .rebuild_seen_cache = false,
            .download_seen_cache = false,
            .download_lightmap = true,
            .vision_block_mask = vision_transparency_block_mask(),
            .angled_sunlight_shadows = angled_sunlight_shadows,
            .direct_sunlight = m_solar.direct_active,
            .sun_dx_per_z = m_solar.dx_per_z,
            .sun_dy_per_z = m_solar.dy_per_z,
        } );
        if( pending_gpu_lighting.id == 0 ) {
            debugmsg( "SDL_GPU lighting dispatch failed; see debug.log for details" );
            return;
        }
        if( submap_loader.has_deferred_lazy_border_work() ) {
            ZoneScopedN( "Phase4_lightmap_pending_lazy_border" );
            submap_loader.process_deferred_lazy_border_work();
        }
    }
#endif

    {
        ZoneScopedN( "Phase3_sound_absorption" );
        // Absorption cache relies upon the floor, outside, and vehicle caches all being completed.
        for( int z = -OVERMAP_DEPTH; z <= OVERMAP_HEIGHT; z++ ) {
            build_absorption_cache( z );
        }
    }

    seen_cache_dirty |= build_vision_transparency_cache( get_player_character() );
    if( seen_cache_dirty ) {
        skew_vision_cache.assign( vision_cache_slots, vision_cache_slot{} );
    }
    const auto need_seen_rebuild = seen_cache_dirty || force_seen_rebuild_for_gpu_residency ||
                                   m_last_seen_cache_origin != p;
    TracyPlot( "Map Need Seen Rebuild", need_seen_rebuild ? int64_t{ 1 } : int64_t{ 0 } );
    if( need_seen_rebuild ) {
#if defined( CATA_SDL )
        if( use_sdl_gpu_compute ) {
            if( gpu_device == nullptr ) {
                debugmsg( "SDL_GPU lighting is required for 3D visibility, but no GPU device is available" );
                return;
            }
            m_last_seen_cache_origin = tripoint_bub_ms( tripoint_min );
        } else {
            build_seen_cache( p, zlev );
            m_last_seen_cache_origin = p;
        }
#else
        build_seen_cache( p, zlev );
        m_last_seen_cache_origin = p;
#endif
        // seen_cache changed (or will be updated by GPU pass); mark visibility stale.
        mark_visibility_cache_dirty( zlev );
    }
    if( !skip_lightmap ) {
#if defined( CATA_SDL )
        if( use_sdl_gpu_compute ) {
            if( gpu_device != nullptr ) {
                if( !dirty_lightmap_levels.empty() ) {
                    ZoneScopedN( "Phase4_lightmap_finish" );
                    const bool gpu_lighting_ok = cata_gpu::finish_gpu_lighting( gpu_device,
                                                 pending_gpu_lighting );
                    if( !gpu_lighting_ok ) {
                        debugmsg( "SDL_GPU lighting completion failed; see debug.log for details" );
                        return;
                    }

                    std::ranges::for_each( dirty_lightmap_levels, [this]( int z ) {
                        get_cache( z ).lightmap_dirty = false;
                        mark_visibility_cache_dirty( z );
                    } );
                }
            } else if( !dirty_lightmap_levels.empty() ) {
                debugmsg( "SDL_GPU lighting is required for lightmap rebuild, but no GPU device is available" );
                return;
            }
        } else
#endif
        {
            if( !dirty_lightmap_levels.empty() ) {
                auto colored_light_levels = std::vector<int> {};
                std::ranges::copy( std::views::iota( -OVERMAP_DEPTH, OVERMAP_HEIGHT + 1 ),
                                   std::back_inserter( colored_light_levels ) );
                for( const auto z : colored_light_levels ) {
                    auto &c = get_cache( z );
                    std::ranges::fill( c.colored_light_cache, 0u );
                    c.colored_light_cache_active = false;
                }

                if( dirty_lightmap_levels.size() > 1 && parallel_enabled && parallel_map_cache ) {
                    // Multiple dirty levels: hoist shared initialization outside the
                    // parallel loop so worker threads never race on cross-level writes.
                    //
                    // Clear sm, light_source_buffer, and lm for every dirty level.
                    // lm must be zeroed because build_sunlight_cache only writes outdoor tiles.
                    for( const int z : dirty_lightmap_levels ) {
                        auto &c = get_cache( z );
                        std::fill( c.sm.begin(), c.sm.end(), 0.0f );
                        std::fill( c.light_source_buffer.begin(), c.light_source_buffer.end(), 0.0f );
                        std::fill( c.colored_light_source_buffer.begin(), c.colored_light_source_buffer.end(),
                                   0.0f );
                        std::fill( c.light_source_color_buffer.begin(), c.light_source_color_buffer.end(), 0u );
                        c.light_source_points.clear();
                        std::ranges::fill( c.lm, 0.0f );
                        c.lm_cpu_cache_valid = false;
                        ++c.lm_cpu_cache_generation;
                    }
                    // Build sunlight (all z-levels, top-to-bottom; serial).
                    build_sunlight_cache( zlev );
                    // Apply character/NPC lights serially to avoid racing on per-level caches.
                    apply_character_light( get_player_character() );
                    for( npc &guy : g->all_npcs() ) {
                        apply_character_light( guy );
                    }
                    // Apply monster lights serially (all_monsters() uses non-atomic weak_ptr_fast
                    // refcounts, so iterating from worker threads would be a data race).
                    for( monster &critter : g->all_monsters() ) {
                        if( critter.is_hallucination() ) {
                            continue;
                        }
                        const auto &mp = critter.bub_pos();
                        if( inbounds( mp ) ) {
                            if( critter.has_effect( effect_onfire ) ) {
                                apply_light_source( mp, 8 );
                            }
                            if( critter.type->luminance > 0 ) {
                                apply_light_source( mp, critter.type->luminance );
                            }
                        }
                    }
                    // Generate per-level dynamic lighting in parallel.
                    // skip_shared_init=true: workers only process entities on their own z-level.
                    // Pre-warm the vehicle list cache serially to avoid heap corruption
                    // from concurrent writes to last_full_vehicle_list.
                    get_vehicles();
                    parallel_for( 0, static_cast<int>( dirty_lightmap_levels.size() ), [&]( int i ) {
                        generate_lightmap_worker( dirty_lightmap_levels[i] );
                    } );
                } else {
                    // Single dirty level: run serially using the standard full path.
                    for( const int level : dirty_lightmap_levels ) {
                        generate_lightmap( level );
                    }
                }

                if( colored_lighting ) {
                    for( const auto z : colored_light_levels ) {
                        auto &c = get_cache( z );
                        c.colored_light_cache_active = std::ranges::any_of(
                        c.colored_light_cache, []( const auto packed ) {
                            return packed != 0u;
                        } );
                    }
                }

                // Mark each regenerated level clean so subsequent redraws this turn skip it.
                // Also mark visibility dirty: the lightmap just changed, so any visibility
                // cache computed before this rebuild (e.g. from handle_action's unconditional
                // update_visibility_cache call) is now stale and must be rebuilt in game::draw.
                std::ranges::for_each( dirty_lightmap_levels, [this]( int z ) {
                    auto &c = get_cache( z );
                    c.lightmap_dirty = false;
                    c.lm_cpu_cache_valid = true;
                    mark_visibility_cache_dirty( z );
                } );

            } // end if( !dirty_lightmap_levels.empty() )
        }
    }
}

submap *map::get_submap_at( const tripoint_bub_ms &p ) const
{
    if( inbounds( p ) ) {
        const auto abs_sm_pos = map_local_to_abs( *this,
                                tripoint_bub_sm( p.x() / SEEX, p.y() / SEEY, p.z() ) );
        return get_mapbuffer().lookup_submap_in_memory( abs_sm_pos );
    }
    if( get_mapbuffer().is_outside_pocket_dimension_bounds( map_local_to_abs( *this, p ) ) ) {
        // Outside dimension bounds — genuinely invalid position.
        return nullptr;
    }
    // Loaded-but-out-of-bubble fallback: look up from the bound dimension's mapbuffer.
    // Uses lookup_submap_in_memory to avoid triggering disk loads from query functions.
    const tripoint_abs_sm abs_sm_pos(
        abs_sub.x() + divide_round_to_minus_infinity( p.x(), SEEX ),
        abs_sub.y() + divide_round_to_minus_infinity( p.y(), SEEY ),
        p.z()
    );
    return get_mapbuffer().lookup_submap_in_memory( abs_sm_pos );
}

submap *map::get_submap_at( const tripoint_bub_ms &p, point_sm_ms &offset_p ) const
{
    // Use floor-division so that negative local coords (out-of-bubble) give the
    // correct submap-local offset in [0, SEEX) rather than a negative value.
    const int smx = divide_round_to_minus_infinity( p.x(), SEEX );
    const int smy = divide_round_to_minus_infinity( p.y(), SEEY );
    offset_p = point_sm_ms( point( p.x() - smx * SEEX, p.y() - smy * SEEY ) );
    return get_submap_at( tripoint_bub_ms( p ) );
}

void map::draw_line_ter( const ter_id &type, const tripoint_bub_ms &p1, const tripoint_bub_ms &p2 )
{
    assert( p1.z() == p2.z() );
    const auto z = p1.z();
    draw_line( [this, type, z]( const point & p ) {
        this->ter_set( tripoint_bub_ms( p.x, p.y, z ), type );
    }, p1.xy().raw(), p2.xy().raw() );
}

void map::draw_line_furn( const furn_id &type, const tripoint_bub_ms &p1,
                          const tripoint_bub_ms &p2 )
{
    assert( p1.z() == p2.z() );
    const auto z = p1.z();
    draw_line( [this, type, z]( const point & p ) {
        this->furn_set( tripoint_bub_ms( p.x, p.y, z ), type );
    }, p1.xy().raw(), p2.xy().raw() );
}

void map::draw_fill_background( const ter_id &type )
{
    // Need to explicitly set caches dirty - set_ter would do it before
    const auto z = get_avatar().abs_pos().z();
    set_transparency_cache_dirty( z );
    set_seen_cache_dirty( z );
    set_outside_cache_dirty( z );
    set_pathfinding_cache_dirty( z );

    get_mapbuffer().fill_terrain( {
        .begin = abs_sub,
        .end = abs_sub + point_rel_sm( my_MAPSIZE, my_MAPSIZE ),
        .terrain = type,
    } );
}

void map::draw_fill_background( ter_id( *f )() )
{
    draw_square_ter( f, tripoint_bub_ms{ 0, 0, -OVERMAP_DEPTH },
                     tripoint_bub_ms( SEEX * my_MAPSIZE - 1,
                                      SEEY * my_MAPSIZE - 1, OVERMAP_HEIGHT ) );
}
void map::draw_fill_background( const weighted_int_list<ter_id> &f )
{
    draw_square_ter( f, tripoint_bub_ms{ 0, 0, -OVERMAP_DEPTH },
                     tripoint_bub_ms( SEEX * my_MAPSIZE - 1,
                                      SEEY * my_MAPSIZE - 1, OVERMAP_HEIGHT ) );
}

void map::draw_square_ter( const ter_id &type, const tripoint_bub_ms &p1,
                           const tripoint_bub_ms &p2 )
{
    assert( p1.z() == p2.z() );
    const auto z = p1.z();
    draw_square( [this, type, z]( const point & p ) {
        this->ter_set( tripoint_bub_ms( p.x, p.y, z ), type );
    }, p1.xy().raw(), p2.xy().raw() );
}

void map::draw_square_furn( const furn_id &type, const tripoint_bub_ms &p1,
                            const tripoint_bub_ms &p2 )
{
    assert( p1.z() == p2.z() );
    const auto z = p1.z();
    draw_square( [this, type, z]( const point & p ) {
        this->furn_set( tripoint_bub_ms( p.x, p.y, z ), type );
    }, p1.xy().raw(), p2.xy().raw() );
}

void map::draw_square_ter( ter_id( *f )(), const tripoint_bub_ms &p1, const tripoint_bub_ms &p2 )
{
    assert( p1.z() == p2.z() );
    const auto z = p1.z();
    draw_square( [this, f, z]( const point & p ) {
        this->ter_set( tripoint_bub_ms( p.x, p.y, z ), f() );
    }, p1.xy().raw(), p2.xy().raw() );
}

void map::draw_square_ter( const weighted_int_list<ter_id> &f, const tripoint_bub_ms &p1,
                           const tripoint_bub_ms &p2 )
{
    assert( p1.z() == p2.z() );
    const auto z = p1.z();
    draw_square( [this, f, z]( const point & p ) {
        const ter_id *tid = f.pick();
        this->ter_set( tripoint_bub_ms( p.x, p.y, z ), tid != nullptr ? *tid : t_null );
    }, p1.xy().raw(), p2.xy().raw() );
}

void map::draw_rough_circle_ter( const ter_id &type, const tripoint_bub_ms &p, int rad )
{
    const auto z = p.z();
    draw_rough_circle( [this, type, z]( const point & p ) {
        this->ter_set( tripoint_bub_ms( p.x, p.y, z ), type );
    }, p.xy().raw(), rad );
}

void map::draw_rough_circle_furn( const furn_id &type, const tripoint_bub_ms &p, int rad )
{
    const auto z = p.z();
    draw_rough_circle( [this, type, z]( const point & p ) {
        this->furn_set( tripoint_bub_ms( p.x, p.y, z ), type );
    }, p.xy().raw(), rad );
}

void map::draw_circle_ter( const ter_id &type, const rl_vec2d &p, int zlev, double rad )
{
    draw_circle( [this, type, zlev]( const point & p ) {
        this->ter_set( tripoint_bub_ms( p.x, p.y, zlev ), type );
    }, p, rad );
}

void map::draw_circle_ter( const ter_id &type, const tripoint_bub_ms &p, int rad )
{
    const auto z = p.z();
    draw_circle( [this, type, z]( const point & p ) {
        this->ter_set( tripoint_bub_ms( p.x, p.y, z ), type );
    }, p.xy().raw(), rad );
}

void map::draw_circle_furn( const furn_id &type, const tripoint_bub_ms &p, int rad )
{
    const auto z = p.z();
    draw_circle( [this, type, z]( const point & p ) {
        this->furn_set( tripoint_bub_ms( p.x, p.y, z ), type );
    }, p.xy().raw(), rad );
}

void map::add_corpse( const tripoint_bub_ms &p )
{
    detached_ptr<item> body;

    const bool isReviveSpecial = one_in( 10 );

    if( !isReviveSpecial ) {
        body = item::make_corpse();
    } else {
        body = item::make_corpse( mon_zombie );
        body->set_flag( flag_REVIVE_SPECIAL );
    }

    put_items_from_loc( item_group_id( "default_zombie_clothes" ), p );
    if( one_in( 3 ) ) {
        put_items_from_loc( item_group_id( "default_zombie_items" ), p );
    }

    add_item_or_charges( p, std::move( body ) );
}

field &map::get_field( const tripoint_bub_ms &p )
{
    return field_at( p );
}

void map::creature_on_trap( Creature &c, const bool may_avoid )
{
    get_mapbuffer().creature_on_trap( c, may_avoid );
}

template<typename Functor>
auto map::function_over( const tripoint_bub_ms &start, const tripoint_bub_ms &end,
                         Functor fun ) const -> void
{
    // start and end are just two points, end can be "before" start
    // Also clip the area to map area
    const auto min = tripoint_bub_ms(
                         std::max( std::min( start.x(), end.x() ), 0 ),
                         std::max( std::min( start.y(), end.y() ), 0 ),
                         std::max( std::min( start.z(), end.z() ), -OVERMAP_DEPTH )
                     );
    const auto max = tripoint_bub_ms(
                         std::min( std::max( start.x(), end.x() ), SEEX * my_MAPSIZE - 1 ),
                         std::min( std::max( start.y(), end.y() ), SEEY * my_MAPSIZE - 1 ),
                         std::min( std::max( start.z(), end.z() ), OVERMAP_HEIGHT )
                     );

    if( min.x() > max.x() || min.y() > max.y() || min.z() > max.z() ) {
        return;
    }

    // Submaps that contain the bounding points
    const auto min_sm = project_to<coords::sm>( min.xy() );
    const auto max_sm = project_to<coords::sm>( max.xy() );
    const auto submap_range = point_range<point_bub_sm>( min_sm, max_sm );

    const auto apply_to_submap = [&]( const tripoint_bub_sm & sm_pos,
                                      const submap * cur_submap, const point_sm_ms & sm_min,
    const point_sm_ms & sm_max ) -> iteration_state {
        for( const auto sm_ms : point_range<point_sm_ms>( sm_min, sm_max ) )
        {
            const auto rval = fun( sm_pos, cur_submap, sm_ms );
            if( rval != ITER_CONTINUE ) {
                return rval;
            }
        }
        return ITER_CONTINUE;
    };

    // Z outermost, because submaps are flat.
    for( const auto z : std::views::iota( min.z(), max.z() + 1 ) ) {
        auto skip_zlevel = false;
        for( const auto smp : submap_range ) {
            if( skip_zlevel ) {
                break;
            }
            const auto sm_pos = tripoint_bub_sm( smp, z );
            const auto view = active_submaps_.get_submap_view( point_rel_sm( smp.x(), smp.y() ), z );
            if( !view ) {
                // This can happen in pocket dimensions where out-of-bounds
                // submaps are intentionally set to null.
                continue;
            }
            const submap &cur_submap = view->get_submap();
            const auto sm_ms_min = project_remain<coords::sm>( min ).remainder;
            const auto sm_ms_max = project_remain<coords::sm>( max ).remainder;

            const auto sm_min = point_sm_ms( smp.x() > min_sm.x() ? 0 : sm_ms_min.x(),
                                             smp.y() > min_sm.y() ? 0 : sm_ms_min.y() );
            const auto sm_max = point_sm_ms( smp.x() < max_sm.x() ? SEEX - 1 : sm_ms_max.x(),
                                             smp.y() < max_sm.y() ? SEEY - 1 : sm_ms_max.y() );
            switch( apply_to_submap( sm_pos, &cur_submap, sm_min, sm_max ) ) {
                case ITER_CONTINUE:
                case ITER_SKIP_SUBMAP:
                    break;
                case ITER_SKIP_ZLEVEL:
                    skip_zlevel = true;
                    break;
                case ITER_FINISH:
                    return;
            }
        }
    }
}

void map::scent_blockers( std::vector<char> &scent_transfer, int st_sy,
                          const tripoint_bub_ms &min, const tripoint_bub_ms &max )
{
    if( st_sy <= 0 ) {
        return;
    }
    const auto scent_cache_x = static_cast<int>( scent_transfer.size() / static_cast<size_t>( st_sy ) );
    const auto reduce = TFLAG_REDUCE_SCENT;
    const auto block = TFLAG_NO_SCENT;
    auto fill_values = [&]( const tripoint_bub_sm & gp, const submap * sm, point_sm_ms lp ) {
        // We need to generate the x/y coordinates, because we can't get them "for free"
        const auto p = project_combine( gp, lp );
        if( sm->get_ter( lp ).obj().has_flag( block ) ) {
            scent_transfer[p.x() * st_sy + p.y()] = 0;
        } else if( sm->get_ter( lp ).obj().has_flag( reduce ) ||
                   sm->get_furn( lp ).obj().has_flag( reduce ) ) {
            scent_transfer[p.x() * st_sy + p.y()] = 1;
        } else {
            scent_transfer[p.x() * st_sy + p.y()] = 5;
        }

        return ITER_CONTINUE;
    };

    function_over( min, max, fill_values );

    const inclusive_cuboid<tripoint_bub_ms> local_bounds( min, max );
    const auto mark_vehicle_obstruction = [&]( const tripoint_bub_ms & part_pos ) {
        if( !local_bounds.contains( part_pos ) || part_pos.x() < 0 || part_pos.y() < 0 ||
            part_pos.x() >= scent_cache_x || part_pos.y() >= st_sy ) {
            return;
        }
        const auto index = static_cast<size_t>( part_pos.x() * st_sy + part_pos.y() );
        if( scent_transfer[index] == 5 ) {
            scent_transfer[index] = 1;
        }
    };

    // Now vehicles

    auto vehs = get_vehicles();
    for( auto &wrapped_veh : vehs ) {
        vehicle &veh = *( wrapped_veh.v );
        for( const vpart_reference &vp : veh.get_any_parts( VPFLAG_OBSTACLE ) ) {
            mark_vehicle_obstruction( vp.pos() );
        }

        // Doors, but only the closed ones
        for( const vpart_reference &vp : veh.get_any_parts( VPFLAG_OPENABLE ) ) {
            if( !vp.part().open ) {
                mark_vehicle_obstruction( vp.pos() );
            }
        }
    }
}

tripoint_range<tripoint_bub_ms> map::points_in_rectangle( const tripoint_bub_ms &from,
        const tripoint_bub_ms &to ) const
{
    const auto bubble_max = bubble_tiles().max();
    const auto min = tripoint_bub_ms( std::max( 0, std::min( from.x(), to.x() ) ),
                                      std::max( 0, std::min( from.y(), to.y() ) ),
                                      std::max( -OVERMAP_DEPTH, std::min( from.z(), to.z() ) ) );
    const auto max = tripoint_bub_ms( std::min( bubble_max.x(), std::max( from.x(), to.x() ) ),
                                      std::min( bubble_max.y(), std::max( from.y(), to.y() ) ),
                                      std::min( OVERMAP_HEIGHT, std::max( from.z(), to.z() ) ) );
    if( min.x() > max.x() || min.y() > max.y() || min.z() > max.z() ) {
        return tripoint_range<tripoint_bub_ms>( tripoint_bub_ms::zero(), tripoint_bub_ms( 0, 0, -1 ) );
    }
    return tripoint_range<tripoint_bub_ms>( min, max );
}

tripoint_range<tripoint_bub_ms> map::points_in_radius( const tripoint_bub_ms &center, size_t radius,
        size_t radiusz ) const
{
    const auto xy_radius = static_cast<int>( radius );
    const auto z_radius = static_cast<int>( radiusz );
    const auto bubble_max = bubble_tiles().max();
    const auto min = tripoint_bub_ms( std::max( 0, center.x() - xy_radius ),
                                      std::max( 0, center.y() - xy_radius ),
                                      clamp( center.z() - z_radius, -OVERMAP_DEPTH, OVERMAP_HEIGHT ) );
    const auto max = tripoint_bub_ms( std::min( bubble_max.x(), center.x() + xy_radius ),
                                      std::min( bubble_max.y(), center.y() + xy_radius ),
                                      clamp( center.z() + z_radius, -OVERMAP_DEPTH, OVERMAP_HEIGHT ) );
    if( min.x() > max.x() || min.y() > max.y() || min.z() > max.z() ) {
        return tripoint_range<tripoint_bub_ms>( tripoint_bub_ms::zero(), tripoint_bub_ms( 0, 0, -1 ) );
    }
    return tripoint_range<tripoint_bub_ms>( min, max );
}

tripoint_range<tripoint_bub_ms> map::points_on_zlevel( const int z ) const
{
    if( z < -OVERMAP_DEPTH || z > OVERMAP_HEIGHT ) {
        // TODO: need a default constructor that creates an empty range.
        return tripoint_range<tripoint_bub_ms>( tripoint_bub_ms::zero(),
                                                tripoint_bub_ms::zero() - tripoint_above );
    }
    return tripoint_range<tripoint_bub_ms>(
               tripoint_bub_ms( 0, 0, z ), tripoint_bub_ms( SEEX * my_MAPSIZE - 1, SEEY * my_MAPSIZE - 1, z ) );
}

tripoint_range<tripoint_bub_ms> map::points_on_zlevel() const
{
    return points_on_zlevel( get_avatar().abs_pos().z() );
}

std::vector<item *> map::get_active_items_in_radius( const tripoint_bub_ms &center,
        int radius ) const
{
    return get_active_items_in_radius( center, radius, special_item_type::none );
}

std::vector<item *> map::get_active_items_in_radius( const tripoint_bub_ms &center, int radius,
        special_item_type type ) const
{
    return get_mapbuffer().get_active_items_in_radius( map_local_to_abs( *this, center ), radius,
            type );
}

std::vector<tripoint_bub_ms> map::find_furnitures_with_flag_in_omt( const tripoint_bub_ms &p,
        const std::string &flag )
{
    // Some stupid code to get to the corner
    const auto omt_p = abs_to_map_local( *this, project_to<coords::ms>(
            project_to<coords::omt>( map_local_to_abs( *this, p ) ) ) );

    std::vector<tripoint_bub_ms> furn_locs;
    for( const auto &furn_loc : points_in_rectangle( omt_p,
            tripoint_bub_ms( omt_p.x() + 2 * SEEX - 1, omt_p.y() + 2 * SEEY - 1, omt_p.z() ) ) ) {
        if( has_flag_furn( flag, furn_loc ) ) {
            furn_locs.push_back( furn_loc );
        }
    }
    return furn_locs;
};

std::list<tripoint_bub_ms> map::find_furnitures_with_flag_in_radius( const tripoint_bub_ms &center,
        size_t radius,
        const std::string &flag,
        size_t radiusz )
{
    std::list<tripoint_bub_ms> furn_locs;
    for( const auto &furn_loc : points_in_radius( center, radius, radiusz ) ) {
        if( has_flag_furn( flag, furn_loc ) ) {
            furn_locs.push_back( furn_loc );
        }
    }
    return furn_locs;
}

std::list<tripoint_bub_ms> map::find_furnitures_or_vparts_with_flag_in_radius(
    const tripoint_bub_ms &center,
    size_t radius, const std::string &flag, size_t radiusz )
{
    std::list<tripoint_bub_ms> locs;
    for( const auto &loc : points_in_radius( center, radius, radiusz ) ) {
        // workaround for ramp bridges
        int dz = 0;
        if( has_flag( TFLAG_RAMP_UP, loc ) ) {
            dz = 1;
        } else if( has_flag( TFLAG_RAMP_DOWN, loc ) ) {
            dz = -1;
        }

        if( dz == 0 ) {
            if( has_flag_furn_or_vpart( flag, loc ) ) {
                locs.push_back( loc );
            }
        } else {
            const auto newloc( loc + tripoint_rel_ms( 0, 0, dz ) );
            if( has_flag_furn_or_vpart( flag, newloc ) ) {
                locs.push_back( newloc );
            }
        }
    }

    return locs;
}

std::list<Creature *> map::get_creatures_in_radius( const tripoint_bub_ms &center, size_t radius,
        size_t radiusz )
{
    std::list<Creature *> creatures;
    for( const auto &loc : points_in_radius( center, radius, radiusz ) ) {
        Creature *tmp_critter = g->critter_at( loc );
        if( tmp_critter != nullptr ) {
            creatures.push_back( tmp_critter );
        }

    }
    return creatures;
}

bool map::has_rope_at( tripoint_bub_ms pt ) const
{
    if( cached_veh_rope.contains( pt ) ) {
        auto veh_pair = get_rope_at( pt );
        vehicle *veh = veh_pair.first;
        int veh_part = veh_pair.second;
        return veh->part( veh_part ).info().ladder_length() >= veh->bub_ms_location().z() - pt.z();
    }
    return false;
}
std::pair<vehicle *, int> map::get_rope_at( const tripoint_bub_ms &pt ) const
{
    return cached_veh_rope.at( pt );
}

level_cache &map::access_cache( int zlev )
{
    if( zlev >= -OVERMAP_DEPTH && zlev <= OVERMAP_HEIGHT ) {
        return *caches[zlev + OVERMAP_DEPTH];
    }

    debugmsg( "access_cache called with invalid z-level: %d", zlev );
    return nullcache;
}

const level_cache &map::access_cache( int zlev ) const
{
    if( zlev >= -OVERMAP_DEPTH && zlev <= OVERMAP_HEIGHT ) {
        return *caches[zlev + OVERMAP_DEPTH];
    }

    debugmsg( "access_cache called with invalid z-level: %d", zlev );
    return nullcache;
}

// Default constructor: zero-sized null sentinel — not for normal use.
level_cache::level_cache() = default;

/// Normal constructor: mx = SEEX * mapsize, my = SEEY * mapsize.
// Tile-coordinate vectors are allocated at the runtime mx * my so that the
// cache correctly tracks the actual loaded-area dimensions.  idx() now uses
// the runtime cache_y stride, matching these allocations.
level_cache::level_cache( int mx, int my )
    : cache_x( mx ), cache_y( my ), cache_mapsize( mx / SEEX ),
      transparency_cache_dirty( static_cast<size_t>( mx / SEEX ) * ( my / SEEY ) ),
      outside_cache_dirty( static_cast<size_t>( mx / SEEX ) * ( my / SEEY ) ),
      floor_cache_dirty( static_cast<size_t>( mx / SEEX ) * ( my / SEEY ) ),
      absorption_cache_dirty( static_cast<size_t>( mx / SEEX ) * ( my / SEEY ) ),
      sound_wall_cache_dirty( static_cast<size_t>( mx / SEEX ) * ( my / SEEY ) ),
      lm( static_cast<size_t>( mx * my ), 0.0f ),
      sm( static_cast<size_t>( mx * my ), 0.0f ),
      light_source_buffer( static_cast<size_t>( mx * my ), 0.0f ),
      colored_light_source_buffer( static_cast<size_t>( mx * my ), 0.0f ),
      light_source_color_buffer( static_cast<size_t>( mx * my ), 0u ),
      outside_cache( static_cast<size_t>( mx * my ), '\0' ),
      sheltered_cache( static_cast<size_t>( mx * my ), '\0' ),
      floor_cache( static_cast<size_t>( mx * my ), false ),
      vehicle_floor_cache( static_cast<size_t>( mx * my ), '\0' ),
      transparency_cache( static_cast<size_t>( mx * my ), 0.0f ),
      vehicle_obscured_cache( static_cast<size_t>( mx * my ), diagonal_blocks{false, false} ),
      vehicle_obstructed_cache( static_cast<size_t>( mx * my ), diagonal_blocks{false, false} ),
      seen_cache( static_cast<size_t>( mx * my ), 0.0f ),
      camera_cache( static_cast<size_t>( mx * my ), 0.0f ),
      visibility_cache( static_cast<size_t>( mx * my ), lit_level::DARK ),
      colored_light_cache( static_cast<size_t>( mx * my ), 0u ),
      map_memory_seen_cache( static_cast<size_t>( mx * my ) ),
      veh_exists_at( static_cast<size_t>( mx * my ), false ),
      absorption_cache( static_cast<size_t>( mx * my ), 0 ),
      sound_wall_cache( static_cast<size_t>( mx * my ), false )

{
    transparency_cache_dirty.set();
    absorption_cache_dirty.set();
    sound_wall_cache_dirty.set();
    outside_cache_dirty.set();
    floor_cache_dirty.set();
}

// Default constructor: zero-sized null sentinel — not for normal use.
sound_instance_cache::sound_instance_cache() = default;

// Normal constructor. Use this in almost all cases, takes the originating sound event.
// This is done so we can garuntee that the sound is sized to the reality bubble and the level_cache.
sound_instance_cache::sound_instance_cache( sound_event &input_sound,
        const sound_vol_for_flood_dist &d_e, const int &f_r )
    : sound( input_sound ),
      dist_enum( d_e ),
      flood_radius( f_r ),
      origin( input_sound.origin ),
      envelope_index_point( tripoint( origin.x() - flood_radius, origin.y() - flood_radius,
                                      origin.z() ) ),
      offset_x( envelope_index_point.x() ),
      offset_y( envelope_index_point.y() ),
      // 4r^2 + 4r + 1 equals our total area, for some (2r + 1) by (2r + 1) flood envelope.
      volume( static_cast<size_t>( ( 1 + ( 4 * flood_radius ) + ( 4 * ( flood_radius *
                                     flood_radius ) ) ) ), 0 ),
      movement_noise( input_sound.movement_noise ),
      from_player( input_sound.from_player ),
      from_monster( input_sound.from_monster ),
      from_npc( input_sound.from_npc )
{

}

sound_filter_key::sound_filter_key() = default;

sound_cache::sound_cache() = default;

void map::set_pathfinding_cache_dirty( const int zlev )
{
    if( !inbounds_z( zlev ) ) {
        return;
    }
    get_mapbuffer().mark_submap_caches_dirty( {
        .begin = abs_sub,
        .end = abs_sub + point_rel_sm( my_MAPSIZE, my_MAPSIZE ),
        .zlev = zlev,
        .pathfinding = true,
    } );
}

void map::set_pathfinding_cache_dirty( const tripoint_bub_ms &p )
{
    if( !inbounds( p ) ) {
        return;
    }
    const auto abs_sm = map_local_to_abs( *this, project_to<coords::sm>( p ) );
    get_mapbuffer().mark_submap_caches_dirty( {
        .begin = abs_sm.xy(),
        .end = abs_sm.xy() + point_rel_sm( 1, 1 ),
        .zlev = abs_sm.z(),
        .pathfinding = true,
    } );
}

auto map::get_pf_special( const tripoint_bub_ms &p ) const -> pf_special
{
    point_sm_ms l;
    submap *sm = get_submap_at( p, l );
    if( !sm ) {
        return PF_WALL;
    }
    if( sm->pf_dirty ) {
        sm->rebuild_pf_cache( *this, project_to<coords::sm>( p ) );
    }
    return sm->pf_special_cache[l.x()][l.y()];
}

bool map::check_seen_cache( const tripoint_bub_ms &p ) const
{
    const level_cache &ch = get_cache( p.z() );
    return !ch.map_memory_seen_cache[ static_cast<size_t>( p.x() + p.y() * ch.cache_x ) ];
}

bool map::check_and_set_seen_cache( const tripoint_bub_ms &p ) const
{
    level_cache &ch = get_cache( p.z() );
    const size_t offset = static_cast<size_t>( p.x() + p.y() * ch.cache_x );
    if( !ch.map_memory_seen_cache[ offset ] ) {
        ch.map_memory_seen_cache.set( offset );
        return true;
    }
    return false;
}

void map::invalidate_map_cache( const int zlev )
{
    if( inbounds_z( zlev ) ) {
        level_cache &ch = get_cache( zlev );
        ch.floor_cache_dirty.set();
        ch.transparency_cache_dirty.set();
        ch.absorption_cache_dirty.set();
        ch.sound_wall_cache_dirty.set();
        ch.seen_cache_dirty = true;
        ch.lightmap_dirty = true;
        ch.lm_cpu_cache_valid = false;
        ++ch.lm_cpu_cache_generation;
        mark_visibility_cache_dirty( zlev );
        ch.outside_cache_dirty.set();
        ch.suspension_cache_dirty = true;
        m_last_seen_cache_origin = tripoint_bub_ms( tripoint_min );
    }
}

void map::invalidate_lightmap_caches()
{
    std::ranges::for_each( std::views::iota( -OVERMAP_DEPTH, OVERMAP_HEIGHT + 1 ), [this]( int z ) {
        auto &cache = get_cache( z );
        cache.lightmap_dirty = true;
        cache.lm_cpu_cache_valid = false;
        ++cache.lm_cpu_cache_generation;
        mark_visibility_cache_dirty( z );
    } );
}

auto map::mark_visibility_cache_dirty( const int zlev ) -> void
{
    if( !inbounds_z( zlev ) ) {
        return;
    }
    get_cache( zlev ).visibility_cache_dirty = true;
    visibility_caches_dirty_ = true;
}

auto map::mark_visibility_caches_clean() -> void
{
    std::ranges::for_each( std::views::iota( -OVERMAP_DEPTH,
    OVERMAP_HEIGHT + 1 ), [this]( const int z ) {
        get_cache( z ).visibility_cache_dirty = false;
    } );
    visibility_caches_dirty_ = false;
}

auto map::visibility_caches_dirty() const -> bool
{
    return visibility_caches_dirty_;
}

auto map::current_lightmap_source_signature() -> std::size_t
{
    ZoneScopedN( "lightmap_source_signature" );

    auto seed = std::size_t{ 0 };

    cata::hash_combine( seed, to_hours<int>( calendar::turn - calendar::turn_zero ) );
    update_solar_params();
    cata::hash_combine( seed, angled_sunlight_shadows );
    cata::hash_combine( seed, m_solar.direct_active );
    if( angled_sunlight_shadows && m_solar.direct_active ) {
        cata::hash_combine( seed, quantized_solar_signature_value( m_solar.dx_per_z ) );
        cata::hash_combine( seed, quantized_solar_signature_value( m_solar.dy_per_z ) );
    }
    std::ranges::for_each( std::views::iota( -OVERMAP_DEPTH, OVERMAP_HEIGHT + 1 ), [&]( const int z ) {
        cata::hash_combine( seed, z );
        cata::hash_combine( seed, quantized_light_signature_value( g->natural_light_level( z ) ) );
    } );

    hash_character_light_state( seed, get_player_character() );
    for( npc &guy : g->all_npcs() ) {
        hash_character_light_state( seed, guy );
    }
    for( monster &critter : g->all_monsters() ) {
        if( critter.is_hallucination() ) {
            continue;
        }
        const auto has_fire_light = critter.has_effect( effect_onfire );
        const auto luminance = critter.type->luminance;
        if( !has_fire_light && luminance <= 0.0f ) {
            continue;
        }
        cata::hash_combine( seed, critter.bub_pos() );
        cata::hash_combine( seed, has_fire_light );
        cata::hash_combine( seed, quantized_light_signature_value( luminance ) );
    }

    for( const auto &view : active_submaps_.submaps() ) {
        const auto grid = abs_to_bub( view.abs_pos() );
        const submap &sm = view.get_submap();
        if( sm.is_uniform ) {
            continue;
        }
        for( const auto local : sm.field_cache ) {
            const auto &curfield = sm.get_field( local );
            if( curfield.field_count() == 0 ) {
                continue;
            }
            const auto pos = project_combine( grid, local );
            for( const auto &field_pair : curfield ) {
                const auto &entry = field_pair.second;
                const auto emitted = entry.light_emitted();
                const auto override = entry.local_light_override();
                if( emitted <= 0 && override < 0.0f ) {
                    continue;
                }
                cata::hash_combine( seed, pos );
                cata::hash_combine( seed, field_pair.first );
                cata::hash_combine( seed, entry.get_field_intensity() );
                cata::hash_combine( seed, quantized_light_signature_value(
                                        static_cast<float>( emitted ) ) );
                cata::hash_combine( seed, quantized_light_signature_value( override ) );
            }
        }
    }

    const auto &luminous_item_submaps = get_mapbuffer().get_submaps_with_luminous_items();
    for( const tripoint_abs_sm &abs_pos : luminous_item_submaps ) {
        if( !contains_abs_sm( abs_pos ) || !submap_loader.is_simulated( bound_dimension_, abs_pos ) ) {
            continue;
        }
        const submap *const sm_ptr = get_mapbuffer().lookup_submap_in_memory( abs_pos );
        if( sm_ptr == nullptr ) {
            continue;
        }
        const submap &sm = *sm_ptr;
        for( const auto sm_ms : submap_tiles() ) {
            if( sm.get_lum( sm_ms ) == 0 ) {
                continue;
            }
            const auto pos = abs_to_map_local( *this, project_combine( abs_pos, sm_ms ) );
            for( const item *const itm : sm.get_items( sm_ms ) ) {
                if( itm != nullptr ) {
                    hash_light_item( seed, pos, *itm );
                }
            }
        }
    }

    const auto odd_turn = calendar::once_every( 2_turns );
    for( const wrapped_vehicle &wrapped : get_vehicles() ) {
        vehicle *veh = wrapped.v;
        if( veh == nullptr ) {
            continue;
        }

        auto lights = veh->lights( true );
        auto veh_luminance = 0.0f;
        auto iteration = 1.0f;
        for( const auto pt : lights ) {
            const auto &vp = pt->info();
            if( vp.has_flag( VPFLAG_CONE_LIGHT ) ||
                vp.has_flag( VPFLAG_WIDE_CONE_LIGHT ) ) {
                veh_luminance += vp.bonus / iteration;
                iteration = iteration * 1.1f;
            }
        }

        for( const auto pt : lights ) {
            const auto &vp = pt->info();
            const auto src = veh->bub_part_location( *pt );
            if( !inbounds( src ) ) {
                continue;
            }
            cata::hash_combine( seed, src );
            cata::hash_combine( seed, quantized_light_signature_value(
                                    static_cast<float>( vp.bonus ) ) );
            cata::hash_combine( seed, quantized_light_signature_value( veh_luminance ) );
            cata::hash_combine( seed, quantized_angle_signature_value(
                                    veh->face.dir() + pt->direction ) );
            cata::hash_combine( seed, vp.has_flag( VPFLAG_CONE_LIGHT ) );
            cata::hash_combine( seed, vp.has_flag( VPFLAG_WIDE_CONE_LIGHT ) );
            cata::hash_combine( seed, vp.has_flag( VPFLAG_HALF_CIRCLE_LIGHT ) );
            cata::hash_combine( seed, vp.has_flag( VPFLAG_CIRCLE_LIGHT ) );
            cata::hash_combine( seed, vp.rotating_light.has_value() );
            if( vp.rotating_light ) {
                const auto base_direction = veh->face.dir() + pt->direction;
                const auto active_direction = vp.rotating_light->direction_at( base_direction,
                                              calendar::turn );
                cata::hash_combine( seed, quantized_angle_signature_value(
                                        vp.rotating_light->arc_width() ) );
                cata::hash_combine( seed, vp.rotating_light->beam_count() );
                cata::hash_combine( seed, quantized_angle_signature_value(
                                        vp.rotating_light->beam_spacing() ) );
                cata::hash_combine( seed, quantized_angle_signature_value( active_direction ) );
                cata::hash_combine( seed, to_turns<int>( vp.rotating_light->period ) );
            }
            const auto uses_flash_gated_light = !vp.rotating_light &&
                                                vp.has_flag( VPFLAG_CIRCLE_LIGHT ) &&
                                                ( vp.has_flag( VPFLAG_ODDTURN ) ||
                                                  vp.has_flag( VPFLAG_EVENTURN ) );
            cata::hash_combine( seed, uses_flash_gated_light );
            if( uses_flash_gated_light ) {
                cata::hash_combine( seed, odd_turn );
            }
        }

        for( const vpart_reference &vp : veh->get_all_parts() ) {
            if( !vp.has_feature( VPFLAG_CARGO ) || vp.has_feature( "COVERED" ) ) {
                continue;
            }
            const auto part_index = static_cast<int>( vp.part_index() );
            const auto pos = vp.pos();
            if( !inbounds( pos ) ) {
                continue;
            }
            for( item * const &itm : veh->get_items( part_index ) ) {
                if( itm != nullptr ) {
                    hash_light_item( seed, pos, *itm );
                }
            }
        }
    }

    return seed;
}

void map::invalidate_lightmap_caches_if_light_state_changed()
{
    const auto signature = current_lightmap_source_signature();
    if( m_last_lightmap_source_signature_valid &&
        signature == m_last_lightmap_source_signature ) {
        TracyPlot( "Light Source Signature Changed", int64_t{ 0 } );
        return;
    }
    TracyPlot( "Light Source Signature Changed", int64_t{ 1 } );
    m_last_lightmap_source_signature = signature;
    m_last_lightmap_source_signature_valid = true;
    invalidate_lightmap_caches();
}

void map::invalidate_visibility_caches()
{
    std::ranges::for_each( std::views::iota( -OVERMAP_DEPTH,
    OVERMAP_HEIGHT + 1 ), [this]( const int z ) {
        mark_visibility_cache_dirty( z );
    } );
}

void map::set_memory_seen_cache_dirty( const tripoint_bub_ms &p )
{
    level_cache &ch = get_cache( p.z() );
    const int offset = p.x() + p.y() * ch.cache_x;
    if( offset >= 0 && offset < ch.cache_x * ch.cache_y ) {
        const auto bit = static_cast<size_t>( offset );
        if( ch.map_memory_seen_cache[bit] ) {
            ch.map_memory_seen_cache.reset( bit );
            if( !ch.map_memory_seen_cache_dirty_all ) {
                ch.map_memory_seen_cache_dirty_points.push_back( p );
            }
        }
    }
}

auto map::set_memory_seen_cache_dirty( const int zlev ) -> void
{
    level_cache &ch = get_cache( zlev );
    ch.map_memory_seen_cache.reset();
    ch.map_memory_seen_cache_dirty_points.clear();
    ch.map_memory_seen_cache_dirty_all = true;
}

auto map::is_memory_seen_cache_dirty_all( const int zlev ) const -> bool
{
    return get_cache( zlev ).map_memory_seen_cache_dirty_all;
}

auto map::take_memory_seen_cache_dirty_points( const int zlev ) -> std::vector<tripoint_bub_ms>
{
    level_cache &ch = get_cache( zlev );
    auto dirty_points = std::move( ch.map_memory_seen_cache_dirty_points );
    ch.map_memory_seen_cache_dirty_points.clear();
    return dirty_points;
}

auto map::mark_memory_seen_cache_dirty_all_clean( const int zlev ) -> void
{
    level_cache &ch = get_cache( zlev );
    ch.map_memory_seen_cache_dirty_all = false;
    ch.map_memory_seen_cache_dirty_points.clear();
}

void map::clip_to_bounds( point_bub_ms &p ) const
{
    constexpr int sms = coords::map_squares_per( coords::scale::submap );
    p.x() = std::clamp( p.x(), 0, sms * my_MAPSIZE - 1 );
    p.y() = std::clamp( p.y(), 0, sms * my_MAPSIZE - 1 );
}

void map::clip_to_bounds( point_bub_sm &p ) const
{
    p.x() = std::clamp( p.x(), 0, my_MAPSIZE - 1 );
    p.y() = std::clamp( p.y(), 0, my_MAPSIZE - 1 );
}

void map::clip_to_bounds( tripoint_bub_ms &p ) const
{
    constexpr int sms = coords::map_squares_per( coords::scale::submap );
    p.x() = std::clamp( p.x(), 0, sms * my_MAPSIZE - 1 );
    p.y() = std::clamp( p.y(), 0, sms * my_MAPSIZE - 1 );
    p.z() = std::clamp( p.z(), -OVERMAP_DEPTH, OVERMAP_HEIGHT );
}

void map::clip_to_bounds( tripoint_bub_sm &p ) const
{
    p.x() = std::clamp( p.x(), 0, my_MAPSIZE - 1 );
    p.y() = std::clamp( p.y(), 0, my_MAPSIZE - 1 );
    p.z() = std::clamp( p.z(), -OVERMAP_DEPTH, OVERMAP_HEIGHT );
}

bool map::is_cornerfloor( const tripoint_bub_ms &p ) const
{
    if( impassable( p ) ) {
        return false;
    }
    std::set<tripoint_bub_ms> impassable_adjacent;
    for( const tripoint_bub_ms &pt : points_in_radius( p, 1 ) ) {
        if( impassable( pt ) ) {
            impassable_adjacent.insert( pt );
        }
    }
    if( !impassable_adjacent.empty() ) {
        //to check if a floor is a corner we first search if any of its diagonal adjacent points is impassable
        std::set< tripoint_bub_ms> diagonals = { p + tripoint_north_east, p + tripoint_north_west, p + tripoint_south_east, p + tripoint_south_west };
        for( const auto &impassable_diagonal : diagonals ) {
            if( impassable_adjacent.contains( impassable_diagonal ) ) {
                //for every impassable diagonal found, we check if that diagonal terrain has at least two impassable neighbors that also neighbor point p
                int f = 0;
                for( const auto &l : points_in_radius( impassable_diagonal, 1 ) ) {
                    if( impassable_adjacent.contains( l ) ) {
                        f++;
                    }
                    if( f > 2 ) {
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

int map::calc_max_populated_zlev()
{
    const int player_z = get_avatar().abs_pos().z();
    const auto cache_key = tripoint_abs_sm( get_abs_sub(), player_z );
    // cache is filled and valid, skip recalculation
    if( max_populated_zlev && max_populated_zlev->first == cache_key ) {
        return max_populated_zlev->second;
    }

    // We'll assume ground level is populated
    auto max_z = 0;

    const auto expected_submaps = static_cast<std::size_t>( my_MAPSIZE ) *
                                  static_cast<std::size_t>( my_MAPSIZE );
    for( const auto zlev : std::views::iota( 1, OVERMAP_HEIGHT + 1 ) ) {
        const auto submaps = active_submaps_.submaps( zlev );
        if( submaps.size() != expected_submaps ||
        std::ranges::any_of( submaps, []( const mapbuffer_abs_submap_view & view ) {
        return !view.get_submap().is_uniform;
        } ) ) {
            max_z = zlev;
        }
    }

    max_populated_zlev = std::pair<tripoint_abs_sm, int>( cache_key, max_z );
    return max_z;
}

void map::invalidate_max_populated_zlev( int zlev )
{
    if( max_populated_zlev && max_populated_zlev->second < zlev ) {
        max_populated_zlev->second = zlev;
    }
}

tripoint_bub_ms drawsq_params::center() const
{
    if( view_center == tripoint_bub_ms( tripoint_min ) ) {
        return g->u.bub_pos() + g->u.view_offset;
    } else {
        return view_center;
    }
}
