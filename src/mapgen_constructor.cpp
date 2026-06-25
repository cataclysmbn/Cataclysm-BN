#include "mapgen_constructor.h"

#include <algorithm>
#include <climits>
#include <memory>
#include <ranges>

#include "artifact.h"
#include "catalua_hooks.h"
#include "catalua_sol.h"
#include "computer.h"
#include "debug.h"
#include "field.h"
#include "field_type.h"
#include "flag.h"
#include "game.h"
#include "game_constants.h"
#include "item.h"
#include "item_category.h"
#include "item_factory.h"
#include "item_group.h"
#include "line.h"
#include "mapbuffer.h"
#include "mapdata.h"
#include "mapgendata.h"
#include "mongroup.h"
#include "npc.h"
#include "omdata.h"
#include "options.h"
#include "overmapbuffer.h"
#include "point.h"
#include "point_float.h"
#include "rng.h"
#include "submap.h"
#include "text_snippets.h"
#include "thread_pool.h"
#include "trap.h"
#include "vehicle.h"
#include "vehicle_group.h"
#include "vehicle_part.h"
#include "vpart_position.h"
#include "veh_type.h"

static const trait_id trait_NPC_STATIC_NPC( "NPC_STATIC_NPC" );
static const mongroup_id GROUP_BREATHER( "GROUP_BREATHER" );
static const mongroup_id GROUP_BREATHER_HUB( "GROUP_BREATHER_HUB" );
static const mtype_id mon_zombie( "mon_zombie" );
static const itype_id itype_diesel( "diesel" );
static const itype_id itype_gasoline( "gasoline" );

namespace
{

static constexpr auto resident_lookup = mapbuffer_lookup_options
{
    .mode = mapbuffer_lookup_mode::resident_only
};

static constexpr auto load_or_generate_lookup = mapbuffer_lookup_options
{
    .mode = mapbuffer_lookup_mode::load_or_generate
};

auto omt_tile_max() -> point_omt_ms
{
    return point_omt_ms( SEEX * 2 - 1, SEEY * 2 - 1 );
}

auto is_inside_omt_tile_bounds( const point_omt_ms &p ) -> bool
{
    return p.x() >= 0 && p.x() < SEEX * 2 &&
           p.y() >= 0 && p.y() < SEEY * 2;
}

auto omt_submap_index( const point_omt_sm &local ) -> std::optional<std::size_t>
{
    if( local.x() < 0 || local.x() > 1 || local.y() < 0 || local.y() > 1 ) {
        return std::nullopt;
    }
    return static_cast<std::size_t>( local.x() + local.y() * 2 );
}

auto has_owned_submaps( const std::vector<std::unique_ptr<submap>> &submaps ) -> bool
{
    return std::ranges::any_of( submaps, []( const auto & sm ) {
        return sm != nullptr;
    } );
}

auto null_items() -> location_vector<item> &
{
    static location_vector<item> nulitems( new fake_item_location() );
    return nulitems;
}

auto max_items_for_tile( const submap &sm, const point_sm_ms &local ) -> units::volume
{
    const auto furniture = sm.get_furn( local );
    if( furniture != f_null ) {
        return furniture->max_volume;
    }
    return sm.get_ter( local )->max_volume;
}

} // namespace

mapgen_constructor::mapgen_constructor( mapbuffer &buffer )
    : buffer_( buffer )
{
}

mapgen_constructor::mapgen_constructor( mapbuffer &buffer, const tripoint_abs_omt &omt_pos )
    : buffer_( buffer )
{
    bind_omt_for_hook( omt_pos );
}

mapgen_constructor::~mapgen_constructor() = default;

auto mapgen_constructor::get_abs_omt() const -> tripoint_abs_omt
{
    return abs_offset_;
}

auto mapgen_constructor::get_bound_dimension() const -> const dimension_id &
{
    return buffer_.get_dimension_id();
}

auto mapgen_constructor::get_buffer() const -> mapbuffer &
{
    return buffer_;
}

auto mapgen_constructor::all_submap_offsets() const -> point_range<point_omt_sm>
{
    return { point_omt_sm::zero(), point_omt_sm( 1, 1 ) };
}

auto mapgen_constructor::all_tile_bounds() const -> inclusive_rectangle<point_omt_ms>
{
    return { point_omt_ms::zero(), omt_tile_max() };
}

auto mapgen_constructor::submap_at( const point_omt_ms &p ) const -> submap *
{
    return submap_at_grid( project_to<coords::sm>( p ) );
}

auto mapgen_constructor::submap_at_grid( const point_omt_sm &gridp ) const -> submap *
{
    const auto abs = project_combine( abs_offset_, gridp );
    const auto index = omt_submap_index( gridp );
    if( index && *index < owned_submaps_.size() && owned_submaps_[*index] != nullptr ) {
        return owned_submaps_[*index].get();
    }
    return buffer_.lookup_submap_in_memory( abs );
}

auto mapgen_constructor::tile_at( const point_omt_ms &p ) const -> std::pair<submap *, point_sm_ms>
{
    return { submap_at( p ), project_remain<coords::sm>( p ).remainder };
}

auto mapgen_constructor::bind_omt_for_hook( const tripoint_abs_omt &omt_pos ) -> bool
{
    clear();
    abs_offset_ = omt_pos;
    return std::ranges::all_of( all_submap_offsets(), [&]( const point_omt_sm & offset ) {
        return buffer_.lookup_submap_in_memory( project_combine( abs_offset_, offset ) ) != nullptr;
    } );
}

auto mapgen_constructor::load( const tripoint_abs_omt &omt_pos ) -> bool
{
    clear();
    abs_offset_ = omt_pos;
    return std::ranges::all_of( all_submap_offsets(), [&]( const point_omt_sm & offset ) {
        return buffer_.get_submap( project_combine( abs_offset_, offset ),
                                   load_or_generate_lookup ) != nullptr;
    } );
}

auto mapgen_constructor::clear() -> void
{
    owned_submaps_.clear();
}

auto mapgen_constructor::for_each_submap( const std::function<void( submap & )> &func ) const ->
void
{
    std::ranges::for_each( all_submap_offsets(), [&]( const point_omt_sm & offset ) {
        submap *const sm = submap_at_grid( offset );
        if( sm != nullptr ) {
            func( *sm );
        }
    } );
}

auto mapgen_constructor::clear_spawns() -> void
{
    for_each_submap( []( submap & sm ) {
        sm.spawns.clear();
    } );
}

auto mapgen_constructor::clear_traps() -> void
{
    for_each_submap( []( submap & sm ) {
        for( const auto p : submap_tiles() ) {
            sm.set_trap( p, tr_null );
        }
    } );
}

auto mapgen_constructor::reset_scratch_omt( const tripoint_abs_omt &omt_pos, const ter_id &terrain,
        const furn_id &furniture, const trap_id &trap ) -> void
{
    clear();
    abs_offset_ = omt_pos;
    owned_submaps_.resize( 4 );
    std::ranges::for_each( all_submap_offsets(), [&]( const point_omt_sm & offset ) {
        const auto index = omt_submap_index( offset );
        if( !index ) {
            return;
        }
        const auto abs = project_combine( abs_offset_, offset );
        auto sm = std::make_unique<submap>( abs, buffer_.get_dimension_id() );
        sm->set_all_ter( terrain );
        sm->set_all_furn( furniture );
        sm->set_all_traps( trap );
        owned_submaps_[*index] = std::move( sm );
    } );
}

auto mapgen_constructor::ensure_omt_submaps( const time_point &when ) -> bool
{
    owned_submaps_.clear();
    owned_submaps_.resize( 4 );
    std::ranges::for_each( all_submap_offsets(), [&]( const point_omt_sm & offset ) {
        const auto index = omt_submap_index( offset );
        if( !index ) {
            return;
        }
        const auto abs = project_combine( abs_offset_, offset );
        if( buffer_.lookup_submap_in_memory( abs ) != nullptr ) {
            return;
        }
        auto sm = std::make_unique<submap>( abs, buffer_.get_dimension_id() );
        sm->last_touched = when;
        owned_submaps_[*index] = std::move( sm );
    } );
    return std::ranges::all_of( all_submap_offsets(), [&]( const point_omt_sm & offset ) {
        return submap_at_grid( offset ) != nullptr;
    } );
}

auto mapgen_constructor::commit_generated_submaps() -> bool
{
    if( !has_owned_submaps( owned_submaps_ ) ) {
        return std::ranges::all_of( all_submap_offsets(), [&]( const point_omt_sm & offset ) {
            const auto *const sm = buffer_.lookup_submap_in_memory( project_combine( abs_offset_, offset ) );
            return sm != nullptr && sm->get_ter( point_sm_ms::zero() ) != t_null;
        } );
    }
    if( std::ranges::any_of( all_submap_offsets(), [&]( const point_omt_sm & offset ) {
    const auto *const sm = submap_at_grid( offset );
        return sm == nullptr || sm->get_ter( point_sm_ms::zero() ) == t_null;
    } ) ) {
        return false;
    }
    if( std::ranges::any_of( all_submap_offsets(), [&]( const point_omt_sm & offset ) {
    const auto index = omt_submap_index( offset );
        return index && *index < owned_submaps_.size() && owned_submaps_[*index] != nullptr &&
               buffer_.lookup_submap_in_memory( project_combine( abs_offset_, offset ) ) != nullptr;
    } ) ) {
        return false;
    }
    for( const point_omt_sm &offset : all_submap_offsets() ) {
        const auto index = omt_submap_index( offset );
        if( !index || *index >= owned_submaps_.size() || owned_submaps_[*index] == nullptr ) {
            continue;
        }
        const auto abs = project_combine( abs_offset_, offset );
        owned_submaps_[*index]->set_position( abs );
        if( !buffer_.add_submap( abs, owned_submaps_[*index] ) ) {
            return false;
        }
    }
    owned_submaps_.clear();
    return true;
}

auto mapgen_constructor::discard_generated_submaps() -> void
{
    owned_submaps_.clear();
}

auto mapgen_constructor::points_on_zlevel() const -> point_range<point_omt_ms>
{
    return { point_omt_ms::zero(), omt_tile_max() };
}

auto mapgen_constructor::points_in_rectangle( const point_omt_ms &from,
        const point_omt_ms &to ) const -> point_range<point_omt_ms>
{
    const auto bounds = all_tile_bounds();
    const auto min = point_omt_ms(
                         std::max( bounds.p_min.x(), std::min( from.x(), to.x() ) ),
                         std::max( bounds.p_min.y(), std::min( from.y(), to.y() ) ) );
    const auto max = point_omt_ms(
                         std::min( bounds.p_max.x(), std::max( from.x(), to.x() ) ),
                         std::min( bounds.p_max.y(), std::max( from.y(), to.y() ) ) );
    return { min, max };
}

auto mapgen_constructor::points_in_radius( const point_omt_ms &center,
        const size_t radius ) const -> point_range<point_omt_ms>
{
    const auto xy_radius = static_cast<int>( radius );
    return points_in_rectangle(
               point_omt_ms( center.x() - xy_radius, center.y() - xy_radius ),
               point_omt_ms( center.x() + xy_radius, center.y() + xy_radius ) );
}

auto mapgen_constructor::ter( const point_omt_ms &p ) const -> ter_id
{
    const auto [sm, local] = tile_at( p );
    return sm != nullptr ? sm->get_ter( local ) : t_null;
}

auto mapgen_constructor::furn( const point_omt_ms &p ) const -> furn_id
{
    const auto [sm, local] = tile_at( p );
    return sm != nullptr ? sm->get_furn( local ) : f_null;
}

auto mapgen_constructor::tr_at( const point_omt_ms &p ) const -> const trap &
{
    const auto [sm, local] = tile_at( p );
    if( sm == nullptr ) {
        return tr_null.obj();
    }
    if( sm->get_ter( local ).obj().trap != tr_null ) {
        return sm->get_ter( local ).obj().trap.obj();
    }
    return sm->get_trap( local ).obj();
}

auto mapgen_constructor::has_furn( const point_omt_ms &p ) const -> bool
{
    return furn( p ) != f_null;
}

auto mapgen_constructor::has_flag( const std::string &flag, const point_omt_ms &p ) const -> bool
{
    return has_flag_ter( flag, p ) || has_flag_furn( flag, p );
}

auto mapgen_constructor::has_flag( const ter_bitflags flag, const point_omt_ms &p ) const -> bool
{
    return has_flag_ter( flag, p ) || has_flag_furn( flag, p );
}

auto mapgen_constructor::has_flag_ter( const std::string &flag,
                                       const point_omt_ms &p ) const -> bool
{
    const auto [sm, local] = tile_at( p );
    return sm != nullptr && sm->get_ter( local ).obj().has_flag( flag );
}

auto mapgen_constructor::has_flag_ter( const ter_bitflags flag,
                                       const point_omt_ms &p ) const -> bool
{
    const auto [sm, local] = tile_at( p );
    return sm != nullptr && sm->get_ter( local ).obj().has_flag( flag );
}

auto mapgen_constructor::has_flag_furn( const std::string &flag,
                                        const point_omt_ms &p ) const -> bool
{
    const auto [sm, local] = tile_at( p );
    return sm != nullptr && sm->get_furn( local ).obj().has_flag( flag );
}

auto mapgen_constructor::has_flag_furn( const ter_bitflags flag,
                                        const point_omt_ms &p ) const -> bool
{
    const auto [sm, local] = tile_at( p );
    return sm != nullptr && sm->get_furn( local ).obj().has_flag( flag );
}

auto mapgen_constructor::passable( const point_omt_ms &p ) const -> bool
{
    return move_cost( p ) > 0;
}

auto mapgen_constructor::impassable( const point_omt_ms &p ) const -> bool
{
    return !passable( p );
}

auto mapgen_constructor::move_cost( const point_omt_ms &p,
                                    const vehicle *ignored_vehicle ) const -> int
{
    const furn_t &furniture = furn( p ).obj();
    const ter_t &terrain = ter( p ).obj();
    const optional_vpart_position vp = veh_at( p );
    const vehicle *const veh = ( !vp || &vp->vehicle() == ignored_vehicle ) ? nullptr : &vp->vehicle();
    const int part = veh != nullptr ? vp->part_index() : -1;

    if( terrain.movecost == 0 || ( furniture.id && furniture.movecost < 0 ) ) {
        return 0;
    }
    if( veh != nullptr ) {
        const vpart_position veh_part( const_cast<vehicle &>( *veh ), part );
        if( veh_part.obstacle_at_part() ) {
            return 0;
        }
        if( veh_part.part_with_feature( VPFLAG_AISLE, true ) ) {
            return 2;
        }
        return 8;
    }
    if( furniture.id ) {
        return std::max( terrain.movecost + furniture.movecost, 0 );
    }
    return std::max( terrain.movecost, 0 );
}

auto mapgen_constructor::is_bashable( const point_omt_ms &p, const bool allow_floor ) const -> bool
{
    const auto &terrain = ter( p ).obj();
    const auto &furniture = furn( p ).obj();
    return furniture.bash.str_max != -1 || terrain.bash.str_max != -1 ||
           ( allow_floor && !terrain.has_flag( TFLAG_NO_FLOOR ) );
}

auto mapgen_constructor::veh_at( const point_omt_ms &p ) const -> optional_vpart_position
{
    const auto abs_p = project_combine( abs_offset_, p );
    if( !has_owned_submaps( owned_submaps_ ) ) {
        return buffer_.veh_at( abs_p, resident_lookup );
    }
    for( const vehicle *const veh : get_vehicles() ) {
        const int part = veh->part_at( abs_p - veh->abs_ms_location() );
        if( part >= 0 ) {
            return optional_vpart_position( vpart_position( *const_cast<vehicle *>( veh ),
                                            static_cast<size_t>( part ) ) );
        }
    }
    return optional_vpart_position( std::nullopt );
}

auto mapgen_constructor::get_vehicles() const -> std::vector<vehicle *>
{
    auto result = std::vector<vehicle *> {};
    std::ranges::for_each( all_submap_offsets(), [&]( const point_omt_sm & offset ) {
        const auto *const sm = submap_at_grid( offset );
        if( sm == nullptr ) {
            return;
        }
        std::ranges::for_each( sm->vehicles, [&]( const auto & veh ) {
            result.push_back( veh.get() );
        } );
    } );
    return result;
}

auto mapgen_constructor::ter_set( const point_omt_ms &p, const ter_id &terrain ) -> bool
{
    const auto [sm, local] = tile_at( p );
    if( sm == nullptr || sm->get_ter( local ) == terrain ) {
        return false;
    }
    sm->set_ter( local, terrain );
    return true;
}

auto mapgen_constructor::furn_set( const point_omt_ms &p, const furn_id &furniture ) -> bool
{
    const auto [sm, local] = tile_at( p );
    if( sm == nullptr || sm->get_furn( local ) == furniture ) {
        return false;
    }
    sm->set_furn( local, furniture );
    const auto &new_furniture = furniture.obj();
    if( new_furniture.active ) {
        cata::poly_serialized<active_tile_data> atd;
        atd.reset( new_furniture.active->clone() );
        atd->set_last_updated( calendar::turn );
        sm->active_furniture[local] = atd;
    }
    return true;
}

auto mapgen_constructor::trap_set( const point_omt_ms &p, const trap_id &trap ) -> void
{
    const auto [sm, local] = tile_at( p );
    if( sm == nullptr || sm->get_ter( local ).obj().trap != tr_null ) {
        return;
    }
    sm->set_trap( local, trap );
}

auto mapgen_constructor::remove_trap( const point_omt_ms &p ) -> void
{
    trap_set( p, tr_null );
}

auto mapgen_constructor::set_radiation( const point_omt_ms &p, const int value ) -> void
{
    const auto [sm, local] = tile_at( p );
    if( sm != nullptr ) {
        sm->set_radiation( local, value );
    }
}

auto mapgen_constructor::adjust_radiation( const point_omt_ms &p, const int delta ) -> void
{
    const auto [sm, local] = tile_at( p );
    if( sm != nullptr ) {
        sm->set_radiation( local, sm->get_radiation( local ) + delta );
    }
}

auto mapgen_constructor::get_temperature( const point_omt_ms &p ) const -> int
{
    const auto *const sm = submap_at( p );
    return sm != nullptr ? sm->get_temperature() : 0;
}

auto mapgen_constructor::set_temperature( const point_omt_ms &p, const int temperature ) -> void
{
    auto *const sm = submap_at( p );
    if( sm != nullptr ) {
        sm->set_temperature( temperature );
    }
}

auto mapgen_constructor::add_field( const point_omt_ms &p, const field_type_id &type_id,
                                    const int intensity, const time_duration &age,
                                    const bool /*hit_player*/ ) -> bool
{
    const auto [sm, local] = tile_at( p );
    if( sm == nullptr || !type_id ) {
        return false;
    }
    const auto field_intensity = std::min( intensity, type_id->get_max_intensity() );
    if( field_intensity <= 0 ) {
        return false;
    }
    const auto added = sm->get_field( local ).add_field(
                           type_id, field_intensity, age );
    sm->is_uniform = false;
    if( added ) {
        sm->field_count++;
        sm->field_cache.push_back( local );
    }
    return true;
}

auto mapgen_constructor::remove_field( const point_omt_ms &p,
                                       const field_type_id &field_to_remove ) -> void
{
    const auto [sm, local] = tile_at( p );
    if( sm != nullptr && sm->get_field( local ).remove_field( field_to_remove ) ) {
        --sm->field_count;
    }
}

auto mapgen_constructor::add_splatter_trail( const field_type_id &type, const point_omt_ms &from,
        const point_omt_ms &to ) -> void
{
    std::ranges::for_each( line_to( from, to ), [&]( const point_omt_ms & p ) {
        add_field( p, type, 1, 0_turns, false );
    } );
}

auto mapgen_constructor::add_computer( const point_omt_ms &p, const std::string &name,
                                       const int security ) -> computer *
{
    ter_set( p, t_console );
    const auto [sm, local] = tile_at( p );
    if( sm == nullptr ) {
        return nullptr;
    }
    sm->set_computer( local, computer( name, security ) );
    return sm->get_computer( local );
}

auto mapgen_constructor::set_signage( const point_omt_ms &p,
                                      const std::string &message ) const -> void
{
    const auto [sm, local] = tile_at( p );
    if( sm != nullptr ) {
        sm->set_signage( local, message );
    }
}

auto mapgen_constructor::set_graffiti( const point_omt_ms &p,
                                       const std::string &contents ) -> void
{
    const auto [sm, local] = tile_at( p );
    if( sm != nullptr ) {
        sm->set_graffiti( local, contents );
    }
}

auto mapgen_constructor::i_at( const point_omt_ms &p ) -> map_stack
{
    const auto [sm, local] = tile_at( p );
    auto *items = sm != nullptr ? &sm->get_items( local ) : nullptr;
    if( items == nullptr ) {
        items = &null_items();
    }
    return map_stack( {
        .stack = items,
        .location = project_combine( abs_offset_, p ),
        .origin = !has_owned_submaps( owned_submaps_ ) ? &buffer_ : nullptr,
        .local_origin = nullptr,
    } );
}

auto mapgen_constructor::i_clear( const point_omt_ms &p ) -> std::vector<detached_ptr<item>>
{
    const auto [sm, local] = tile_at( p );
    if( sm == nullptr ) {
        return {};
    }
    for( item *const it : sm->get_items( local ) ) {
        sm->active_items.remove( it );
    }
    sm->set_lum( local, 0 );
    return sm->get_items( local ).clear();
}

auto mapgen_constructor::add_item( const point_omt_ms &p, detached_ptr<item> &&new_item ) -> void
{
    if( !new_item || new_item->is_null() ) {
        return;
    }
    const auto [sm, local] = tile_at( p );
    if( sm == nullptr ) {
        return;
    }
    sm->is_uniform = false;
    sm->update_lum_add( local, *new_item );
    if( new_item->needs_processing() ) {
        sm->active_items.add( *new_item );
    }
    sm->get_items( local ).push_back( std::move( new_item ) );
}

auto mapgen_constructor::add_item_or_charges( const point_omt_ms &pos,
        detached_ptr<item> &&obj, const bool overflow ) -> detached_ptr<item>
{
    if( !obj || obj->is_null() || obj->has_flag( flag_NO_DROP ) ) {
        return std::move( obj );
    }
    const auto try_place = [&]( const point_omt_ms & p, const bool reject_noitem ) -> bool {
        const auto [sm, local] = tile_at( p );
        if( sm == nullptr || has_flag( "DESTROY_ITEM", p ) ||
            ( obj->made_of( LIQUID ) && has_flag( "SWIMMABLE", p ) ) )
        {
            return false;
        }
        if( reject_noitem && ( has_flag( "NOITEM", p ) || has_flag( "SEALED", p ) ) )
        {
            return false;
        }
        auto &items = sm->get_items( local );
        if( obj->count_by_charges() )
        {
            for( auto &existing : items ) {
                if( existing->merge_charges( std::move( obj ) ) ) {
                    return true;
                }
            }
        }
        auto stored_volume = 0_ml;
        for( const auto *const existing : items )
        {
            stored_volume += existing->volume();
        }
        if( ( has_flag( "NOITEM", p ) && !( has_flag( "LIQUIDCONT", p ) && obj->made_of( LIQUID ) ) ) ||
            obj->volume() > max_items_for_tile( *sm, local ) - stored_volume ||
            items.size() >= MAX_ITEM_IN_SQUARE )
        {
            return false;
        }
        add_item( p, std::move( obj ) );
        return true;
    };
    if( try_place( pos, false ) ) {
        return std::move( obj );
    }
    if( overflow ) {
        for( const auto &candidate : points_in_radius( pos, 2 ) ) {
            if( candidate != pos && try_place( candidate, true ) ) {
                return std::move( obj );
            }
        }
    }
    return std::move( obj );
}

auto mapgen_constructor::spawn_an_item( const point_omt_ms &p, detached_ptr<item> &&new_item,
                                        const int charges, const int damlevel ) -> detached_ptr<item>
{
    if( !new_item ) {
        return detached_ptr<item>();
    }
    if( one_in( 3 ) && new_item->has_flag( flag_VARSIZE ) ) {
        new_item->set_flag( flag_FIT );
    }
    if( charges && new_item->charges > 0 ) {
        new_item->charges = charges;
    }
    auto spawned_item = item::in_its_container( std::move( new_item ) );
    if( ( spawned_item->made_of( LIQUID ) && has_flag( "SWIMMABLE", p ) ) ||
        has_flag( "DESTROY_ITEM", p ) ) {
        return detached_ptr<item>();
    }
    spawned_item->set_damage( damlevel );
    return add_item_or_charges( p, std::move( spawned_item ) );
}

auto mapgen_constructor::spawn_items( const point_omt_ms &p,
                                      std::vector<detached_ptr<item>> new_items ) -> std::vector<detached_ptr<item>>
{
    auto rejected = std::vector<detached_ptr<item>> {};
    std::ranges::for_each( new_items, [&]( detached_ptr<item> &it ) {
        if( auto left = add_item_or_charges( p, std::move( it ) ) ) {
            rejected.push_back( std::move( left ) );
        }
    } );
    return rejected;
}

auto mapgen_constructor::spawn_item( const point_omt_ms &p, const itype_id &type_id,
                                     const unsigned quantity, const int charges,
                                     const time_point &birthday, const int damlevel ) -> void
{
    std::ranges::for_each( std::views::iota( 0u, quantity ), [&]( const auto ) {
        spawn_an_item( p, item::spawn( type_id, birthday ), charges, damlevel );
    } );
}

auto mapgen_constructor::put_items_from_loc( const item_group_id &loc, const point_omt_ms &p,
        const time_point &turn ) -> std::vector<item *>
{
    auto items = item_group::items_from( loc, turn );
    auto ret = std::vector<item *> {};
    ret.reserve( items.size() );
    std::ranges::transform( items, std::back_inserter( ret ),
    []( const detached_ptr<item> &it ) {
        return &*it;
    } );
    spawn_items( p, std::move( items ) );
    return ret;
}

auto mapgen_constructor::item_category_spawn_rate( const item &itm ) -> float
{
    const auto cat_id = item_category_id( itm.get_category_id() );
    return cat_id.is_valid() ? cat_id->get_spawn_rate() : 1.0f;
}

auto mapgen_constructor::flammable_items_at( const point_omt_ms &p, const int threshold ) -> bool
{
    auto items = i_at( p );
    return std::ranges::any_of( items, [threshold]( const item * const it ) {
        return it != nullptr && it->flammable( threshold );
    } );
}

auto mapgen_constructor::place_items( const item_group_id &loc, const int chance,
                                      const point_omt_ms &p1, const point_omt_ms &p2, const bool ongrass,
                                      const time_point &turn, const int magazine, const int ammo ) -> std::vector<item *>
{
    auto res = std::vector<item *> {};
    auto it = itype_id {};
    auto is_type = false;
    if( chance > 100 || chance <= 0 ) {
        debugmsg( "mapgen_constructor::place_items() called with invalid chance %d", chance );
        return res;
    }
    if( !item_group::group_is_defined( loc ) ) {
        it = itype_id( loc.str() );
        if( !it.is_valid() ) {
            const oter_id &oid = get_overmapbuffer( get_bound_dimension() ).ter( get_abs_omt() );
            debugmsg( "place_items: invalid item group / item '%s', om_terrain = '%s' (%s)",
                      loc.c_str(), oid.id().c_str(), oid->get_mapgen_id().c_str() );
            return res;
        }
        is_type = true;
    }

    const auto spawn_rate = get_option<float>( "ITEM_SPAWNRATE" );
    const auto spawn_count = roll_remainder( chance * spawn_rate / 100.0f );
    std::ranges::for_each( std::views::iota( 0, spawn_count ), [&]( const auto ) {
        auto tries = 0;
        auto p = point_omt_ms::zero();
        const auto is_invalid_terrain = [&]( const point_omt_ms & candidate ) {
            const auto &terrain = ter( candidate ).obj();
            return terrain.movecost == 0 &&
                   !terrain.has_flag( "PLACE_ITEM" ) &&
                   !ongrass &&
                   !terrain.has_flag( "FLAT" );
        };
        do {
            p.x() = rng( p1.x(), p2.x() );
            p.y() = rng( p1.y(), p2.y() );
            ++tries;
        } while( is_invalid_terrain( p ) && tries < 20 );
        if( tries >= 20 ) {
            return;
        }
        if( is_type ) {
            auto placed = add_item_or_charges( p, item::spawn( it ) );
            if( placed ) {
                res.push_back( &*placed );
            }
            return;
        }
        auto initial = item_group::items_from( loc, turn );
        std::ranges::for_each( initial, [&]( detached_ptr<item> &itm ) {
            const auto cat_rate = item_category_spawn_rate( *itm );
            if( cat_rate <= 1.0f && rng_float( 0.1f, 1.0f ) > cat_rate ) {
                return;
            }
            auto placed = add_item_or_charges( p, std::move( itm ) );
            if( placed ) {
                res.push_back( &*placed );
            }
        } );
    } );

    std::ranges::for_each( res, [&]( item * const e ) {
        if( e == nullptr || ( !e->is_tool() && !e->is_gun() && !e->is_magazine() ) ) {
            return;
        }
        if( rng( 0, 99 ) < magazine && !e->magazine_current() &&
            e->magazine_default() != itype_id::NULL_ID() ) {
            e->put_in( item::spawn( e->magazine_default(), e->birthday() ) );
        }
        if( rng( 0, 99 ) < ammo && e->ammo_remaining() == 0 ) {
            e->ammo_set( e->ammo_default(), e->ammo_capacity() );
        }
    } );
    return res;
}

auto mapgen_constructor::draw_line_ter( const ter_id &type, const point_omt_ms &p1,
                                        const point_omt_ms &p2 ) -> void
{
    draw_line( [this, type]( const point & p ) {
        ter_set( point_omt_ms( p ), type );
    }, p1.raw(), p2.raw() );
}

auto mapgen_constructor::draw_line_furn( const furn_id &type, const point_omt_ms &p1,
        const point_omt_ms &p2 ) -> void
{
    draw_line( [this, type]( const point & p ) {
        furn_set( point_omt_ms( p ), type );
    }, p1.raw(), p2.raw() );
}

auto mapgen_constructor::draw_fill_background( const ter_id &type ) -> void
{
    std::ranges::for_each( all_submap_offsets(), [&]( const point_omt_sm & offset ) {
        auto *const sm = submap_at_grid( offset );
        if( sm == nullptr ) {
            return;
        }
        sm->is_uniform = true;
        sm->set_all_ter( type );
    } );
}

auto mapgen_constructor::draw_fill_background( ter_id( *f )() ) -> void
{
    draw_square_ter( f, point_omt_ms::zero(), omt_tile_max() );
}

auto mapgen_constructor::draw_fill_background( const weighted_int_list<ter_id> &f ) -> void
{
    draw_square_ter( f, point_omt_ms::zero(), omt_tile_max() );
}

auto mapgen_constructor::draw_square_ter( const ter_id &type, const point_omt_ms &p1,
        const point_omt_ms &p2 ) -> void
{
    draw_square( [this, type]( const point & p ) {
        ter_set( point_omt_ms( p ), type );
    }, p1.raw(), p2.raw() );
}

auto mapgen_constructor::draw_square_ter( ter_id( *f )(), const point_omt_ms &p1,
        const point_omt_ms &p2 ) -> void
{
    draw_square( [this, f]( const point & p ) {
        ter_set( point_omt_ms( p ), f() );
    }, p1.raw(), p2.raw() );
}

auto mapgen_constructor::draw_square_ter( const weighted_int_list<ter_id> &f,
        const point_omt_ms &p1, const point_omt_ms &p2 ) -> void
{
    draw_square( [this, &f]( const point & p ) {
        const auto *const tid = f.pick();
        ter_set( point_omt_ms( p ), tid != nullptr ? *tid : t_null );
    }, p1.raw(), p2.raw() );
}

auto mapgen_constructor::draw_square_furn( const furn_id &type, const point_omt_ms &p1,
        const point_omt_ms &p2 ) -> void
{
    draw_square( [this, type]( const point & p ) {
        furn_set( point_omt_ms( p ), type );
    }, p1.raw(), p2.raw() );
}

auto mapgen_constructor::draw_rough_circle_ter( const ter_id &type, const point_omt_ms &p,
        const int rad ) -> void
{
    draw_rough_circle( [this, type]( const point & loc ) {
        ter_set( point_omt_ms( loc ), type );
    }, p.raw(), rad );
}

auto mapgen_constructor::draw_rough_circle_furn( const furn_id &type, const point_omt_ms &p,
        const int rad ) -> void
{
    draw_rough_circle( [this, type]( const point & loc ) {
        furn_set( point_omt_ms( loc ), type );
    }, p.raw(), rad );
}

auto mapgen_constructor::draw_circle_ter( const ter_id &type, const rl_vec2d &p,
        const double rad ) -> void
{
    draw_circle( [this, type]( const point & loc ) {
        ter_set( point_omt_ms( loc ), type );
    }, p, rad );
}

auto mapgen_constructor::draw_circle_ter( const ter_id &type, const point_omt_ms &p,
        const int rad ) -> void
{
    draw_circle( [this, type]( const point & loc ) {
        ter_set( point_omt_ms( loc ), type );
    }, p.raw(), rad );
}

auto mapgen_constructor::draw_circle_furn( const furn_id &type, const point_omt_ms &p,
        const int rad ) -> void
{
    draw_circle( [this, type]( const point & loc ) {
        furn_set( point_omt_ms( loc ), type );
    }, p.raw(), rad );
}

auto mapgen_constructor::translate( const ter_id &from, const ter_id &to ) -> void
{
    std::ranges::for_each( points_on_zlevel(), [&]( const point_omt_ms & p ) {
        if( ter( p ) == from ) {
            ter_set( p, to );
        }
    } );
}

auto mapgen_constructor::rotate( int turns, const bool setpos_safe ) -> void
{
    turns = turns % 4;
    if( turns == 0 ) {
        return;
    }

    const auto abs_omt = get_abs_omt();
    constexpr auto radius = coords::map_squares_per( coords::scale::overmap_terrain );
    auto &omap = get_overmapbuffer( get_bound_dimension() );
    const auto npcs = omap.get_npcs_near( project_to<coords::sm>( abs_offset_ ), radius );
    std::ranges::for_each( npcs, [&]( const shared_ptr_fast<npc> &i ) {
        npc &np = *i;
        const auto proj = project_remain<coords::omt>( np.abs_pos() );
        if( proj.quotient_tripoint != abs_omt ) {
            return;
        }
        const auto new_pos = project_combine( abs_omt,
                                              proj.remainder.rotate( turns, { SEEX * 2, SEEY * 2 } ) );
        if( setpos_safe ) {
            np.setpos( new_pos );
            return;
        }
        shared_ptr_fast<npc> npc_ptr = omap.remove_npc( np.getID() );
        const auto split = project_remain<coords::sm>( new_pos );
        np.spawn_at_precise( split.quotient, split.remainder_tripoint );
        omap.insert_npc( npc_ptr );
    } );

    const auto swap_submaps = [&]( const point_omt_sm & p1, const point_omt_sm & p2 ) {
        auto *const sm1 = submap_at_grid( p1 );
        auto *const sm2 = submap_at_grid( p2 );
        if( sm1 != nullptr && sm2 != nullptr ) {
            submap::swap( *sm1, *sm2 );
        }
    };

    if( turns == 2 ) {
        swap_submaps( point_omt_sm::zero(), point_omt_sm::south_east() );
        swap_submaps( point_omt_sm::south(), point_omt_sm::east() );
    } else {
        auto p = point_omt_sm::zero();
        const auto p2 = p.rotate( turns, { 2, 2 } );
        const auto p3 = p2.rotate( turns, { 2, 2 } );
        const auto p4 = p3.rotate( turns, { 2, 2 } );
        swap_submaps( p, p2 );
        swap_submaps( p, p3 );
        swap_submaps( p, p4 );
    }

    std::ranges::for_each( all_submap_offsets(), [&]( const point_omt_sm & offset ) {
        auto *const sm = submap_at_grid( offset );
        if( sm == nullptr ) {
            return;
        }
        sm->rotate( turns );
        sm->set_position( project_combine( abs_offset_, offset ) );
        std::ranges::for_each( sm->vehicles, [&]( const auto & veh ) {
            veh->abs_sm_pos = project_combine( abs_offset_, offset );
            veh->refresh_position();
        } );
    } );
}

auto mapgen_constructor::place_spawns( const mongroup_id &group, const int chance,
                                       const point_omt_ms &p1, const point_omt_ms &p2, const float density,
                                       const bool individual, const bool friendly, const std::string &name,
                                       const int mission_id ) -> void
{
    if( !group.is_valid() ) {
        const oter_id &oid = get_overmapbuffer( get_bound_dimension() ).ter( get_abs_omt() );
        debugmsg( "place_spawns: invalid mongroup '%s', om_terrain = '%s' (%s)", group.c_str(),
                  oid.id().c_str(), oid->get_mapgen_id().c_str() );
        return;
    }
    if( !one_in( chance ) ) {
        return;
    }
    const auto spawn_density = MonsterGroupManager::is_animal( group )
                               ? get_option<float>( "SPAWN_ANIMAL_DENSITY" )
                               : get_option<float>( "SPAWN_DENSITY" );
    auto num = roll_remainder( individual ? 1.0f : density * spawn_density * rng_float( 10.0f,
                               50.0f ) );
    while( num > 0 ) {
        auto tries = 10;
        auto p = point_omt_ms::zero();
        do {
            p.x() = rng( p1.x(), p2.x() );
            p.y() = rng( p1.y(), p2.y() );
            --tries;
        } while( impassable( p ) && tries > 0 );
        auto spawn_details = MonsterGroupManager::GetResultFromGroup( group, &num );
        add_spawn( spawn_details.name, spawn_details.pack_size, p,
                   friendly, -1, mission_id, name );
    }
}

auto mapgen_constructor::place_gas_pump( const point_omt_ms &p, const int charges,
        const itype_id &fuel_type ) -> void
{
    auto fuel = item::spawn( fuel_type, calendar::start_of_cataclysm );
    fuel->charges = charges;
    ter_set( p, fuel->fuel_pump_terrain() );
    add_item( p, std::move( fuel ) );
}

auto mapgen_constructor::place_gas_pump( const point_omt_ms &p, const int charges ) -> void
{
    place_gas_pump( p, charges, one_in( 4 ) ? itype_diesel : itype_gasoline );
}

auto mapgen_constructor::place_toilet( const point_omt_ms &p, const int charges ) -> void
{
    auto water = item::spawn( "water", calendar::start_of_cataclysm );
    water->charges = charges;
    add_item( p, std::move( water ) );
    furn_set( p, f_toilet );
}

auto mapgen_constructor::place_vending( const point_omt_ms &p, const item_group_id &type,
                                        const bool reinforced ) -> void
{
    if( reinforced ) {
        furn_set( p, f_vending_reinforced );
        place_items( type, 100, p, p, false, calendar::start_of_cataclysm );
        return;
    }
    if( one_in( 2 ) ) {
        furn_set( p, f_vending_o );
        std::ranges::for_each( points_in_radius( p, 1 ),
        [&]( const point_omt_ms & loc ) {
            if( one_in( 4 ) ) {
                spawn_item( loc, itype_id( "glass_shard" ), rng( 1, 2 ) );
            }
        } );
        return;
    }
    furn_set( p, f_vending_c );
    place_items( type, 100, p, p, false, calendar::start_of_cataclysm );
}

auto mapgen_constructor::place_npc( const point_omt_ms &p, const string_id<npc_template> &type,
                                    const bool force ) -> character_id
{
    if( !force && !get_option<bool>( "STATIC_NPC" ) ) {
        return character_id();
    }
    shared_ptr_fast<npc> temp = make_shared_fast<npc>();
    const auto split = project_remain<coords::sm>( project_combine( abs_offset_, p ) );
    temp->load_npc_template( type );
    temp->spawn_at_precise( split.quotient, split.remainder_tripoint );
    temp->toggle_trait( trait_NPC_STATIC_NPC );
    get_overmapbuffer( get_bound_dimension() ).insert_npc( temp );
    if( !is_pool_worker_thread() ) {
        cata::run_hooks( "on_creature_spawn", [&]( sol::table & params ) {
            params["creature"] = temp.get();
        } );
        cata::run_hooks( "on_npc_spawn", [&]( sol::table & params ) {
            params["npc"] = temp.get();
        } );
    }
    return temp->getID();
}

auto mapgen_constructor::apply_faction_ownership( const point_omt_ms &p1, const point_omt_ms &p2,
        const faction_id &id ) -> void
{
    std::ranges::for_each( points_in_rectangle( p1, p2 ),
    [&]( const point_omt_ms & p ) {
        auto items = i_at( p );
        std::ranges::for_each( items, [&]( item * const elem ) {
            elem->set_owner( id );
        } );
        const auto vp = veh_at( p );
        if( vp && !vp->vehicle().has_owner() ) {
            vp->vehicle().set_owner( id );
        }
    } );
}

auto mapgen_constructor::add_spawn( const mtype_id &type, const int count,
                                    const point_omt_ms &p, const bool friendly, const int faction_id,
                                    const int mission_id, const std::string &name ) const -> void
{
    add_spawn( type, count, p, spawn_point::friendly_to_spawn_disposition( friendly ), faction_id,
               mission_id, name );
}

auto mapgen_constructor::add_spawn( const mtype_id &type, const int count,
                                    const point_omt_ms &p, const spawn_disposition disposition,
                                    const int faction_id, const int mission_id, const std::string &name ) const -> void
{
    auto *const sm = submap_at( p );
    if( sm == nullptr ) {
        debugmsg( "add_spawn target submap missing for %s at %s", type.c_str(), p.to_string() );
        return;
    }
    if( MonsterGroupManager::monster_is_blacklisted( type ) ) {
        return;
    }
    const auto offset = project_remain<coords::sm>( p ).remainder;
    sm->spawns.emplace_back( type, count, offset, faction_id, mission_id, disposition, name );
}

auto mapgen_constructor::add_vehicle( const std::variant<vgroup_id, vproto_id> &type_,
                                      const point_omt_ms &p, const units::angle dir,
                                      const int veh_fuel, const int veh_status, const bool /*merge_wrecks*/,
                                      std::optional<bool> locked, std::optional<bool> has_keys ) -> vehicle *
{
    const auto type = std::visit( []( const auto & v ) -> vproto_id {
        using T = std::decay_t<decltype( v )>;
        if constexpr( std::is_same_v<T, vgroup_id> )
        {
            return v.obj().pick();
        } else
        {
            return v;
        }
    }, type_ );
    if( !is_inside_omt_tile_bounds( p ) || !type.is_valid() ) {
        return nullptr;
    }
    auto veh = std::make_unique<vehicle>( type, veh_fuel, veh_status, locked, has_keys );
    const auto split = project_remain<coords::sm>( project_combine( abs_offset_, p ) );
    veh->abs_sm_pos = split.quotient_tripoint;
    veh->sm_ms_pos = split.remainder;
    veh->place_spawn_items();
    veh->set_facing_and_pivot( dir, tripoint_mnt_veh::zero(), false );
    veh->attach();
    veh->refresh_position();

    auto *const place_on_submap = submap_at_grid( project_remain<coords::omt>
                                  ( veh->abs_sm_pos ).remainder );
    if( place_on_submap == nullptr ) {
        return nullptr;
    }
    auto *const result = veh.get();
    place_on_submap->vehicles.push_back( std::move( veh ) );
    place_on_submap->is_uniform = false;
    return result;
}

auto mapgen_constructor::destroy_vehicle( vehicle *veh ) -> void
{
    if( veh == nullptr ) {
        return;
    }
    auto *const sm = submap_at_grid( project_remain<coords::omt>( veh->abs_sm_pos ).remainder );
    if( sm == nullptr ) {
        return;
    }
    auto iter = std::ranges::find_if( sm->vehicles, [&]( const auto & candidate ) {
        return candidate.get() == veh;
    } );
    if( iter != sm->vehicles.end() ) {
        sm->vehicles.erase( iter );
    }
}

auto mapgen_constructor::add_corpse( const point_omt_ms &p ) -> void
{
    const auto revives = one_in( 10 );
    auto body = revives ? item::make_corpse( mon_zombie ) : item::make_corpse();
    if( revives && body ) {
        body->set_flag( flag_REVIVE_SPECIAL );
    }
    put_items_from_loc( item_group_id( "default_zombie_clothes" ), p );
    if( one_in( 3 ) ) {
        put_items_from_loc( item_group_id( "default_zombie_items" ), p );
    }
    add_item_or_charges( p, std::move( body ) );
}

auto mapgen_constructor::spawn_artifact( const point_omt_ms &p ) -> void
{
    add_item_or_charges( p, item::spawn( new_artifact(), calendar::start_of_cataclysm ) );
}

auto mapgen_constructor::spawn_natural_artifact( const point_omt_ms &p,
        const artifact_natural_property prop ) -> void
{
    add_item_or_charges( p, item::spawn( new_natural_artifact( prop ),
                                         calendar::start_of_cataclysm ) );
}

auto mapgen_constructor::make_rubble( const point_omt_ms &p, const furn_id &rubble_type,
                                      const ter_id &floor_type, const bool overwrite ) -> void
{
    if( overwrite ) {
        ter_set( p, floor_type );
        furn_set( p, rubble_type );
        return;
    }

    if( has_furn( p ) && is_bashable( p ) ) {
        furn_set( p, f_null );
    }
    if( impassable( p ) && is_bashable( p ) ) {
        destroy( p );
    }
    if( impassable( p ) ) {
        ter_set( p, floor_type );
    }

    furn_set( p, rubble_type );
}

auto mapgen_constructor::make_rubble( const point_omt_ms &p,
                                      const furn_id &rubble_type ) -> void
{
    make_rubble( p, rubble_type, t_dirt, false );
}

auto mapgen_constructor::bash( const point_omt_ms &p, const int /*str*/,
                               const bool destroy, const bool /*bash_floor*/,
                               const vehicle * /*bashing_vehicle*/ ) -> void
{
    if( has_furn( p ) ) {
        furn_set( p, f_null );
        return;
    }
    if( destroy || ter( p ).obj().bash.str_max != -1 ) {
        ter_set( p, t_dirt );
    }
}

auto mapgen_constructor::destroy( const point_omt_ms &p ) -> void
{
    int count = 0;
    while( count <= 25 && impassable( p ) ) {
        bash( p, 999, true );
        count++;
    }
}

auto mapgen_constructor::create_anomaly( const point_omt_ms &cp,
        const artifact_natural_property prop, const bool create_rubble ) -> void
{
    if( create_rubble ) {
        draw_rough_circle_ter( t_dirt, cp, 11 );
        draw_rough_circle_furn( f_rubble, cp, 5 );
        furn_set( cp, f_null );
    }

    const auto around = [&]( const int radius ) {
        return points_in_rectangle(
                   cp + point_rel_ms( -radius, -radius ),
                   cp + point_rel_ms( radius, radius ) );
    };

    switch( prop ) {
        case ARTPROP_WRIGGLING:
        case ARTPROP_MOVING:
            std::ranges::for_each( around( 5 ), [&]( const point_omt_ms & p ) {
                if( furn( p ) == f_rubble ) {
                    add_field( p, fd_push_items, 1 );
                }
            } );
            break;

        case ARTPROP_GLOWING:
        case ARTPROP_GLITTERING:
            std::ranges::for_each( around( 5 ), [&]( const point_omt_ms & p ) {
                if( furn( p ) == f_rubble && one_in( 2 ) ) {
                    trap_set( p, tr_glow );
                }
            } );
            break;

        case ARTPROP_HUMMING:
        case ARTPROP_RATTLING:
            std::ranges::for_each( around( 5 ), [&]( const point_omt_ms & p ) {
                if( furn( p ) == f_rubble && one_in( 2 ) ) {
                    trap_set( p, tr_hum );
                }
            } );
            break;

        case ARTPROP_WHISPERING:
        case ARTPROP_ENGRAVED:
            std::ranges::for_each( around( 5 ), [&]( const point_omt_ms & p ) {
                if( furn( p ) == f_rubble && one_in( 3 ) ) {
                    trap_set( p, tr_shadow );
                }
            } );
            break;

        case ARTPROP_BREATHING:
            std::ranges::for_each( around( 1 ), [&]( const point_omt_ms & p ) {
                if( p == cp ) {
                    place_spawns( GROUP_BREATHER_HUB, 1, p, p, 1, true );
                } else {
                    place_spawns( GROUP_BREATHER, 1, p, p, 1, true );
                }
            } );
            break;

        case ARTPROP_DEAD:
            std::ranges::for_each( around( 5 ), [&]( const point_omt_ms & p ) {
                if( furn( p ) == f_rubble ) {
                    trap_set( p, tr_drain );
                }
            } );
            break;

        case ARTPROP_ITCHY:
            std::ranges::for_each( around( 5 ), [&]( const point_omt_ms & p ) {
                if( furn( p ) == f_rubble ) {
                    set_radiation( p, rng( 0, 10 ) );
                }
            } );
            break;

        case ARTPROP_ELECTRIC:
        case ARTPROP_CRACKLING:
            add_field( cp, fd_shock_vent, 3 );
            break;

        case ARTPROP_SLIMY:
            add_field( cp, fd_acid_vent, 3 );
            break;

        case ARTPROP_WARM:
            std::ranges::for_each( around( 5 ), [&]( const point_omt_ms & p ) {
                if( furn( p ) == f_rubble ) {
                    add_field( p, fd_fire_vent, 1 + ( rl_dist( cp, p ) % 3 ) );
                }
            } );
            break;

        case ARTPROP_SCALED:
            std::ranges::for_each( around( 5 ), [&]( const point_omt_ms & p ) {
                if( furn( p ) == f_rubble ) {
                    trap_set( p, tr_snake );
                }
            } );
            break;

        case ARTPROP_FRACTAL:
            create_anomaly( cp + point_rel_ms( -4, -4 ),
                            static_cast<artifact_natural_property>( rng( ARTPROP_NULL + 1, ARTPROP_MAX - 1 ) ) );
            create_anomaly( cp + point_rel_ms( 4, -4 ),
                            static_cast<artifact_natural_property>( rng( ARTPROP_NULL + 1, ARTPROP_MAX - 1 ) ) );
            create_anomaly( cp + point_rel_ms( -4, 4 ),
                            static_cast<artifact_natural_property>( rng( ARTPROP_NULL + 1, ARTPROP_MAX - 1 ) ) );
            create_anomaly( cp + point_rel_ms( 4, -4 ),
                            static_cast<artifact_natural_property>( rng( ARTPROP_NULL + 1, ARTPROP_MAX - 1 ) ) );
            break;

        default:
            break;
    }
}

auto points_in_range( const mapgen_constructor &m ) -> point_range<point_omt_ms>
{
    return m.points_on_zlevel();
}

auto random_point( const point_range<point_omt_ms> &range,
                   const std::function<bool( const point_omt_ms & )> &predicate )
-> std::optional<point_omt_ms> // *NOPAD*
{
    for( const auto unused : std::views::iota( 0, 10 ) ) {
        ( void )unused;
        const auto p = point_omt_ms( rng( range.min().x(), range.max().x() ),
                                     rng( range.min().y(), range.max().y() ) );
        if( predicate( p ) ) {
            return p;
        }
    }
    auto suitable = std::vector<point_omt_ms> {};
    std::ranges::copy_if( range, std::back_inserter( suitable ), predicate );
    if( suitable.empty() ) {
        return std::nullopt;
    }
    return random_entry( suitable );
}

auto random_point( const mapgen_constructor &m,
                   const std::function<bool( const point_omt_ms & )> &predicate )
-> std::optional<point_omt_ms> // *NOPAD*
{
    return random_point( points_in_range( m ), predicate );
}
