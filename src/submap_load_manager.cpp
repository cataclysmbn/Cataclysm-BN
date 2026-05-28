#include "submap_load_manager.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <future>
#include <ranges>
#include <set>
#include <utility>
#include <vector>

#include "calendar.h"
#include "cata_cartesian_product.h"
#include "cached_options.h"
#include "game_constants.h"
#include "mapbuffer.h"
#include "clzones.h"
#include "mapgen_async.h"
#include "mapgen_functions.h"
#include "mapbuffer_registry.h"
#include "omdata.h"
#include "overmapbuffer.h"
#include "point.h"
#include "profile.h"
#include "thread_pool.h"

namespace
{
static constexpr auto retained_omt_min_soft_cap = std::size_t { 16 };
static constexpr auto retained_omt_soft_scale = std::size_t { 4 };
static constexpr auto retained_omt_hard_scale = std::size_t { 2 };
static constexpr auto retained_omt_panic_scale = std::size_t { 4 };
static constexpr auto retained_omt_max_budget_scale = std::size_t { 8 };
static constexpr auto lazy_border_steps_to_cross_omt = std::size_t { SEEX * 2 };

auto divide_round_up_size( const std::size_t numerator, const std::size_t denominator )
-> std::size_t
{
    return ( numerator + denominator - 1 ) / denominator;
}

auto signum( const int value ) -> int
{
    return ( value > 0 ) - ( value < 0 );
}

auto steps_to_omt_edge_axis( const int local, const int direction ) -> std::size_t
{
    const auto clamp_steps = []( const int steps ) -> std::size_t {
        return static_cast<std::size_t>( std::max( 1, steps ) );
    };

    if( direction > 0 ) {
        return clamp_steps( static_cast<int>( lazy_border_steps_to_cross_omt ) - local );
    }
    if( direction < 0 ) {
        return clamp_steps( local + 1 );
    }
    return lazy_border_steps_to_cross_omt;
}

struct omt_edge_deadlines {
    std::size_t x = lazy_border_steps_to_cross_omt;
    std::size_t y = lazy_border_steps_to_cross_omt;
    std::size_t next = lazy_border_steps_to_cross_omt;
};

auto omt_edge_deadlines_for( const tripoint_abs_ms &pos,
                             const point &direction ) -> omt_edge_deadlines
{
    if( direction == point_zero ) {
        return {};
    }

    const auto local = project_remain<coords::omt>( pos.xy() ).remainder;
    auto result = omt_edge_deadlines {
        .x = steps_to_omt_edge_axis( local.x(), direction.x ),
        .y = steps_to_omt_edge_axis( local.y(), direction.y ),
        .next = lazy_border_steps_to_cross_omt
    };
    result.next = std::min( result.x, result.y );
    return result;
}

auto turns_to_omt_edge( const tripoint_abs_ms &pos, const point &direction ) -> std::size_t
{
    return omt_edge_deadlines_for( pos, direction ).next;
}

auto is_omt_zlevel_loaded( mapbuffer &mb, const tripoint_abs_omt &omt_addr ) -> bool
{
    const auto sm_base = project_to<coords::sm>( omt_addr );
    return mb.lookup_submap_in_memory( sm_base )
           && mb.lookup_submap_in_memory( sm_base + point_east )
           && mb.lookup_submap_in_memory( sm_base + point_south )
           && mb.lookup_submap_in_memory( sm_base + point_south_east );
}

auto is_any_omt_zlevel_loaded( mapbuffer &mb, const tripoint_abs_omt &omt_addr ) -> bool
{
    const auto sm_base = project_to<coords::sm>( omt_addr );
    return mb.lookup_submap_in_memory( sm_base )
           || mb.lookup_submap_in_memory( sm_base + point_east )
           || mb.lookup_submap_in_memory( sm_base + point_south )
           || mb.lookup_submap_in_memory( sm_base + point_south_east );
}
} // namespace

submap_load_manager submap_loader;

load_request_handle submap_load_manager::request_load(
    load_request_source source,
    const std::string &dim_id,
    const tripoint_abs_sm &center,
    int radius )
{
    const load_request_handle handle = next_handle_++;
    submap_load_request req;
    req.source = source;
    req.dimension_id = dim_id;
    req.center = center;
    req.radius = radius;
    requests_[handle] = std::move( req );
    return handle;
}

void submap_load_manager::update_request( load_request_handle handle,
        const tripoint_abs_sm &new_center )
{
    auto it = requests_.find( handle );
    if( it == requests_.end() ) {
        return;
    }
    it->second.center = new_center;
}

auto submap_load_manager::update_lazy_border_focus( const std::string &dim_id,
        const tripoint_abs_ms &pos ) -> void
{
    if( lazy_omt_focus_ && lazy_omt_focus_->dimension_id == dim_id ) {
        const auto delta = point{
            signum( pos.x() - lazy_omt_focus_->pos.x() ),
            signum( pos.y() - lazy_omt_focus_->pos.y() )
        };
        if( delta != point_zero ) {
            if( delta != lazy_omt_preload_direction_ ) {
                lazy_omt_budget_credit_ = 0.0;
            }
            lazy_omt_preload_direction_ = delta;
        }
    } else {
        lazy_omt_budget_credit_ = 0.0;
    }

    lazy_omt_focus_ = lazy_omt_focus{
        .dimension_id = dim_id,
        .pos = pos
    };
}

void submap_load_manager::release_load( load_request_handle handle )
{
    if( const auto it = requests_.find( handle );
        it != requests_.end() && it->second.source == load_request_source::lazy_border ) {
        lazy_omt_jobs_.clear();
        lazy_omt_job_index_.clear();
        lazy_omt_budget_credit_ = 0.0;
        lazy_omt_last_credit_turn_ = -1;
    }
    requests_.erase( handle );
}

auto submap_load_manager::update_load_shape( int radius ) -> void
{
    const auto axis = std::views::iota( -radius, radius + 1 );
    bubble_offsets_.clear();
    std::ranges::for_each( cata::views::cartesian_product( axis, axis ),
    [&]( auto pair ) {
        auto [dx, dy] = pair;
        bubble_offsets_.emplace_back( dx, dy );
    } );
}

auto submap_load_manager::compute_desired_set() const -> key_set
{
    ZoneScoped;
    key_set desired;
    std::ranges::for_each( requests_, [&]( const auto & kv ) {
        const submap_load_request &req = kv.second;
        // lazy_border positions are handled separately in OMT space.
        if( req.source == load_request_source::lazy_border ) {
            return;
        }
        // The desired set is 2-D (horizontal only).  Load requests always cover
        // the full z-range; the z-level loop runs inside update() when omts are
        // actually loaded or evicted.
        const point_abs_sm c = req.center.xy();

        if( req.source == load_request_source::reality_bubble ) {
            // Use the precomputed square offsets so all submaps in the full
            // (2*radius+1)×(2*radius+1) grid are protected from eviction.
            // bubble_offsets_ is populated by update_load_shape() in map::resize().
            std::ranges::for_each( bubble_offsets_, [&]( const point & off ) {
                desired.emplace( req.dimension_id, c + off );
            } );
        } else {
            // Other sources (player_base, script, fire_spread) also use square.
            const int r = req.radius;
            const auto axis = std::views::iota( -r, r + 1 );
            std::ranges::for_each(
                cata::views::cartesian_product( axis, axis ),
            [&]( auto pair ) {
                auto [dx, dy] = pair;
                desired.emplace( req.dimension_id, c + point{ dx, dy } );
            } );
        }
    } );
    return desired;
}

auto submap_load_manager::compute_lazy_border_omts() const -> horizontal_omt_set
{
    ZoneScoped;
    auto border_omts = horizontal_omt_set {};
    std::ranges::for_each( requests_, [&]( const auto & kv ) {
        const auto &req = kv.second;
        if( req.source != load_request_source::lazy_border ) {
            return;
        }
        // The lazy border is defined in OMT space: the current bubble's OMT
        // footprint, expanded by one horizontal OMT in every direction.
        const auto c = req.center.xy();
        const auto r = req.radius;
        const auto min_omt = project_to<coords::omt>( point_abs_sm{ c.x() - r, c.y() - r } );
        const auto max_omt = project_to<coords::omt>( point_abs_sm{ c.x() + r, c.y() + r } );
        const auto x_range = std::views::iota( min_omt.x() - 1, max_omt.x() + 2 );
        const auto y_range = std::views::iota( min_omt.y() - 1, max_omt.y() + 2 );
        std::ranges::for_each( cata::views::cartesian_product( x_range, y_range ),
        [&]( const auto pair ) {
            auto [x, y] = pair;
            if( x >= min_omt.x() && x <= max_omt.x() &&
                y >= min_omt.y() && y <= max_omt.y() ) {
                return;
            }
            border_omts.emplace( req.dimension_id, point_abs_omt{ x, y } );
        } );
    } );
    return border_omts;
}

auto submap_load_manager::add_lazy_border_into( key_set &target,
        const horizontal_omt_set &border_omts ) const -> void
{
    ZoneScoped;
    std::ranges::for_each( border_omts, [&]( const retained_omt_key & key ) {
        const auto &[dim_id, omt_xy] = key;
        const auto sm_base = project_to<coords::sm>( omt_xy );
        for( const point &off : { point_zero, point_south, point_east, point_south_east } ) {
            target.emplace( dim_id, sm_base + off );
        }
    } );
}

auto submap_load_manager::current_reality_bubble_radius() const -> int
{
    auto radius = 0;
    std::ranges::for_each( requests_, [&]( const auto & kv ) {
        const auto &req = kv.second;
        if( req.source == load_request_source::reality_bubble ) {
            radius = std::max( radius, req.radius );
        }
    } );
    return radius;
}

auto submap_load_manager::retained_omt_soft_cap() const -> std::size_t
{
    const auto radius = static_cast<std::size_t>( std::max( 0, current_reality_bubble_radius() ) );
    const auto base_cap = std::max( retained_omt_min_soft_cap, radius * retained_omt_soft_scale );
    const auto multiplier = static_cast<std::size_t>(
                                std::clamp( retained_omt_cache_multiplier, 1, 20 ) + 1 );
    return divide_round_up_size( base_cap * multiplier, 2 );
}

auto submap_load_manager::retained_omt_hard_cap() const -> std::size_t
{
    return retained_omt_soft_cap() * retained_omt_hard_scale;
}

auto submap_load_manager::retained_omt_panic_cap() const -> std::size_t
{
    return retained_omt_hard_cap() * retained_omt_panic_scale;
}

auto submap_load_manager::retained_omt_base_budget() const -> std::size_t
{
    const auto radius = static_cast<std::size_t>( std::max( 0, current_reality_bubble_radius() ) );
    return std::max( std::size_t{ 1 }, radius / 6 );
}

auto submap_load_manager::retain_omt( const retained_omt_key &key ) -> void
{
    if( auto it = retained_omt_index_.find( key ); it != retained_omt_index_.end() ) {
        retained_omts_.splice( retained_omts_.end(), retained_omts_, it->second );
        return;
    }
    auto it = retained_omts_.insert( retained_omts_.end(), key );
    retained_omt_index_.emplace( key, it );
}

auto submap_load_manager::erase_retained_omt( const retained_omt_key &key ) -> void
{
    const auto it = retained_omt_index_.find( key );
    if( it == retained_omt_index_.end() ) {
        return;
    }
    retained_omts_.erase( it->second );
    retained_omt_index_.erase( it );
}

auto submap_load_manager::erase_desired_retained_omts( const key_set &desired ) -> void
{
    std::ranges::for_each( desired, [&]( const desired_key & key ) {
        erase_retained_omt( { key.first, project_to<coords::omt>( key.second ) } );
    } );
}

auto submap_load_manager::evict_omt_column( const retained_omt_key &key ) -> void
{
    const auto &[dim_id, omt_xy] = key;
    auto &mb = MAPBUFFER_REGISTRY.get( dim_id );
    std::ranges::for_each( std::views::iota( -OVERMAP_DEPTH, OVERMAP_HEIGHT + 1 ),
    [&]( const auto z ) {
        const auto omt_addr = tripoint_abs_omt{ omt_xy, z };
        const auto qk = omt_key{ dim_id, omt_addr };
        finish_lazy_omt_job( qk );
        const auto was_dirty = dirty_omts_.contains( qk );
        if( was_dirty ) {
            dirty_omts_.erase( qk );
            mb.unload_omt( omt_addr, true );
        } else {
            mb.unload_omt( omt_addr, false );
        }
    } );
}

auto submap_load_manager::evict_oldest_retained_omts( std::size_t count ) -> void
{
    while( count > 0 && !retained_omts_.empty() ) {
        const auto key = retained_omts_.front();
        retained_omts_.pop_front();
        retained_omt_index_.erase( key );
        evict_omt_column( key );
        --count;
    }
}

auto submap_load_manager::process_retained_omt_eviction() -> void
{
    ZoneScopedN( "slm_retained_omt_eviction" );
    const auto retained = retained_omt_index_.size();
    TracyPlot( "Retained OMT Columns", static_cast<int64_t>( retained ) );

    const auto soft_cap = retained_omt_soft_cap();
    if( retained <= soft_cap ) {
        TracyPlot( "Retained OMT Evict Budget", int64_t{ 0 } );
        return;
    }

    const auto hard_cap = retained_omt_hard_cap();
    const auto panic_cap = retained_omt_panic_cap();
    auto budget = retained_omt_base_budget();
    if( retained > panic_cap ) {
        budget = retained - hard_cap;
    } else {
        if( retained > hard_cap ) {
            const auto scale = std::min( retained_omt_max_budget_scale,
                                         divide_round_up_size( retained, hard_cap ) );
            budget *= scale;
        }
        budget = std::min( budget, retained - soft_cap );
    }

    TracyPlot( "Retained OMT Evict Budget", static_cast<int64_t>( budget ) );
    evict_oldest_retained_omts( budget );
}

auto submap_load_manager::load_lazy_omt_zlevel_data( mapbuffer &mb,
        const tripoint_abs_omt &omt_addr,
        const lazy_omt_load_options &options ) -> lazy_omt_load_result
{
    ZoneScopedN( "slm_lazy_load_omt_zlevel_data" );
    auto result = lazy_omt_load_result {};
    if( is_omt_zlevel_loaded( mb, omt_addr ) ) {
        return result;
    }

    result.dirty = mb.preload_omt( omt_addr );
    if( !is_omt_zlevel_loaded( mb, omt_addr ) ) {
        // Partial z-levels are rare, but resolving them can discard duplicate
        // generated submaps; keep that on the main thread.
        if( is_pool_worker_thread() && is_any_omt_zlevel_loaded( mb, omt_addr ) ) {
            return result;
        }
        result.generation = mb.generate_omt( omt_addr, {
            .defer_postprocess_hooks = options.defer_postprocess_hooks,
            .worker_safe = options.worker_safe,
            .use_selected_mapgen = options.use_selected_mapgen,
            .selected_mapgen = options.selected_mapgen,
        } );
    }
    return result;
}

auto submap_load_manager::complete_lazy_omt_result_on_main_thread( const omt_key &key,
        lazy_omt_load_result result ) -> lazy_omt_load_result
{
    if( !result.generation.needs_main_thread() ) {
        return result;
    }

    auto &mb = MAPBUFFER_REGISTRY.get( key.first );
    auto completed = load_lazy_omt_zlevel_data( mb, key.second, {
        .defer_postprocess_hooks = true,
        .use_selected_mapgen = true,
        .selected_mapgen = result.generation.selected_mapgen,
    } );
    completed.dirty = completed.dirty || result.dirty;
    return completed;
}

auto submap_load_manager::erase_lazy_omt_job( const omt_key &key ) -> void
{
    const auto it = lazy_omt_job_index_.find( key );
    if( it == lazy_omt_job_index_.end() ) {
        return;
    }
    lazy_omt_jobs_.erase( it->second );
    lazy_omt_job_index_.erase( it );
}

auto submap_load_manager::apply_lazy_omt_result( const omt_key &key,
        const lazy_omt_load_result &result ) -> bool
{
    MAPBUFFER_REGISTRY.get( key.first ).drain_pending_submap_destroy();
    if( result.dirty || result.generated() ) {
        dirty_omts_.insert( key );
    }
    return result.generated();
}

auto submap_load_manager::finish_lazy_omt_job( const omt_key &key ) -> bool
{
    erase_lazy_omt_job( key );
    const auto it = lazy_omt_futures_.find( key );
    if( it == lazy_omt_futures_.end() ) {
        return false;
    }

    auto result = lazy_omt_load_result {};
    {
        ZoneScopedN( "slm_lazy_z_wait" );
        result = it->second.get();
    }
    result = complete_lazy_omt_result_on_main_thread( key, std::move( result ) );
    const auto generated = apply_lazy_omt_result( key, result );
    lazy_omt_futures_.erase( it );
    if( generated ) {
        run_deferred_mapgen_hooks();
        flush_deferred_zones();
        run_deferred_autonotes();
    }
    return generated;
}

auto submap_load_manager::reap_lazy_omt_jobs() -> void
{
    ZoneScopedN( "slm_lazy_z_reap" );
    auto generated = false;
    auto completed = std::size_t{ 0 };
    std::erase_if( lazy_omt_futures_, [&]( auto & entry ) {
        auto &[key, future] = entry;
        if( future.wait_for( std::chrono::seconds( 0 ) ) != std::future_status::ready ) {
            return false;
        }
        auto result = complete_lazy_omt_result_on_main_thread( key, future.get() );
        generated |= apply_lazy_omt_result( key, result );
        ++completed;
        return true;
    } );

    if( generated ) {
        run_deferred_mapgen_hooks();
        flush_deferred_zones();
        run_deferred_autonotes();
    }
    TracyPlot( "Lazy Border Z Jobs Completed", static_cast<int64_t>( completed ) );
    TracyPlot( "Lazy Border Z Jobs In-Flight", static_cast<int64_t>( lazy_omt_futures_.size() ) );
}

auto submap_load_manager::start_lazy_omt_job( const omt_key &key ) -> bool
{
    if( lazy_omt_futures_.contains( key ) ) {
        return false;
    }

    auto &mb = MAPBUFFER_REGISTRY.get( key.first );
    if( is_omt_zlevel_loaded( mb, key.second ) ) {
        return false;
    }

    if( get_thread_pool().num_workers() == 0 || is_any_omt_zlevel_loaded( mb, key.second ) ) {
        auto result = load_lazy_omt_zlevel_data( mb, key.second, {
            .defer_postprocess_hooks = true,
        } );
        apply_lazy_omt_result( key, result );
        return true;
    }

    if( mapgen_has_any_direct_lua_generator() ) {
        const auto terrain_type = get_overmapbuffer( key.first ).ter( key.second );
        const auto mapgen_id = terrain_type->get_mapgen_id();
        if( mapgen_id_has_direct_lua_generator( mapgen_id ) ) {
            const auto selected_mapgen = pick_mapgen_func( mapgen_id );
            if( mapgen_function_needs_main_thread( selected_mapgen ) ) {
                auto result = load_lazy_omt_zlevel_data( mb, key.second, {
                    .defer_postprocess_hooks = true,
                    .use_selected_mapgen = true,
                    .selected_mapgen = selected_mapgen,
                } );
                apply_lazy_omt_result( key, result );
                return true;
            }

            lazy_omt_futures_.emplace( key,
            get_thread_pool().submit_returning( [&mb, omt_addr = key.second, selected_mapgen]() {
                return load_lazy_omt_zlevel_data( mb, omt_addr, {
                    .defer_postprocess_hooks = true,
                    .worker_safe = true,
                    .use_selected_mapgen = true,
                    .selected_mapgen = selected_mapgen,
                } );
            } ) );
            return true;
        }
    }

    lazy_omt_futures_.emplace( key,
    get_thread_pool().submit_returning( [&mb, omt_addr = key.second]() {
        return load_lazy_omt_zlevel_data( mb, omt_addr, {
            .defer_postprocess_hooks = true,
            .worker_safe = true,
        } );
    } ) );
    return true;
}

auto submap_load_manager::lazy_omt_priority( const retained_omt_key &key ) const -> int
{
    if( lazy_omt_preload_direction_ == point_zero ) {
        return 0;
    }

    auto prioritize_x = lazy_omt_preload_direction_.x != 0;
    auto prioritize_y = lazy_omt_preload_direction_.y != 0;
    if( lazy_omt_focus_ ) {
        const auto deadlines = omt_edge_deadlines_for( lazy_omt_focus_->pos,
                               lazy_omt_preload_direction_ );
        prioritize_x = prioritize_x && deadlines.x == deadlines.next;
        prioritize_y = prioritize_y && deadlines.y == deadlines.next;
    }

    auto best = 0;
    const auto &[dim_id, omt_xy] = key;
    std::ranges::for_each( requests_, [&]( const auto & kv ) {
        const auto &req = kv.second;
        if( req.source != load_request_source::lazy_border || req.dimension_id != dim_id ) {
            return;
        }
        const auto c = req.center.xy();
        const auto r = req.radius;
        const auto min_omt = project_to<coords::omt>( point_abs_sm{ c.x() - r, c.y() - r } );
        const auto max_omt = project_to<coords::omt>( point_abs_sm{ c.x() + r, c.y() + r } );

        auto score = 0;
        if( prioritize_x && lazy_omt_preload_direction_.x > 0 && omt_xy.x() > max_omt.x() ) {
            score += 2;
        } else if( prioritize_x && lazy_omt_preload_direction_.x < 0 && omt_xy.x() < min_omt.x() ) {
            score += 2;
        }

        if( prioritize_y && lazy_omt_preload_direction_.y > 0 && omt_xy.y() > max_omt.y() ) {
            score += 2;
        } else if( prioritize_y && lazy_omt_preload_direction_.y < 0 && omt_xy.y() < min_omt.y() ) {
            score += 2;
        }

        best = std::max( best, score );
    } );
    return best;
}

auto submap_load_manager::lazy_omt_priority( const omt_key &key ) const -> int
{
    return lazy_omt_priority( retained_omt_key{ key.first, key.second.xy() } );
}

auto submap_load_manager::queue_lazy_border_omts( const horizontal_omt_set &border_omts ) -> void
{
    ZoneScopedN( "slm_queue_lazy_border_omts" );

    auto candidates = std::vector<retained_omt_key> {};
    std::ranges::for_each( border_omts, [&]( const retained_omt_key & key ) {
        candidates.push_back( key );
    } );
    std::ranges::sort( candidates, [&]( const retained_omt_key & lhs,
    const retained_omt_key & rhs ) {
        const auto lhs_priority = lazy_omt_priority( lhs );
        const auto rhs_priority = lazy_omt_priority( rhs );
        if( lhs_priority != rhs_priority ) {
            return lhs_priority > rhs_priority;
        }
        if( lhs.first != rhs.first ) {
            return lhs.first < rhs.first;
        }
        if( lhs.second.x() != rhs.second.x() ) {
            return lhs.second.x() < rhs.second.x();
        }
        return lhs.second.y() < rhs.second.y();
    } );

    lazy_omt_jobs_.clear();
    lazy_omt_job_index_.clear();
    std::ranges::for_each( candidates, [&]( const retained_omt_key & key ) {
        const auto &[dim_id, omt_xy] = key;
        auto &mb = MAPBUFFER_REGISTRY.get( dim_id );
        std::ranges::for_each( std::views::iota( -OVERMAP_DEPTH, OVERMAP_HEIGHT + 1 ),
        [&]( const auto z ) {
            const auto omt_addr = tripoint_abs_omt{ omt_xy, z };
            const auto job_key = omt_key{ dim_id, omt_addr };
            if( lazy_omt_futures_.contains( job_key ) || is_omt_zlevel_loaded( mb, omt_addr ) ) {
                return;
            }
            auto it = lazy_omt_jobs_.insert( lazy_omt_jobs_.end(), job_key );
            lazy_omt_job_index_.emplace( job_key, it );
        } );
    } );
}

auto submap_load_manager::process_lazy_border_preload() -> void
{
    ZoneScopedN( "slm_lazy_border_preload" );
    const auto queued = lazy_omt_jobs_.size();
    TracyPlot( "Lazy Border OMT Queue", static_cast<int64_t>( queued ) );
    TracyPlot( "Lazy Border Z Jobs Queue", static_cast<int64_t>( queued ) );
    TracyPlot( "Lazy Border Z Jobs In-Flight", static_cast<int64_t>( lazy_omt_futures_.size() ) );
    if( queued == 0 ) {
        TracyPlot( "Lazy Border OMT Budget", int64_t{ 0 } );
        TracyPlot( "Lazy Border Z Jobs Started", int64_t{ 0 } );
        return;
    }

    const auto urgent = static_cast<std::size_t>( std::ranges::count_if(
    lazy_omt_jobs_, [&]( const omt_key & key ) {
        return lazy_omt_priority( key ) > 0;
    } ) );
    TracyPlot( "Lazy Border Leading OMTs", static_cast<int64_t>( urgent ) );
    TracyPlot( "Lazy Border Leading Z Jobs", static_cast<int64_t>( urgent ) );

    const auto load_budget_matching = [&]( std::size_t budget, const auto & can_load ) -> std::size_t {
        auto started = std::size_t{ 0 };
        while( budget > 0 && !lazy_omt_jobs_.empty() )
        {
            const auto key = lazy_omt_jobs_.front();
            if( !can_load( key ) ) {
                return started;
            }
            erase_lazy_omt_job( key );
            auto &mb = MAPBUFFER_REGISTRY.get( key.first );
            if( is_omt_zlevel_loaded( mb, key.second ) ) {
                continue;
            }
            if( start_lazy_omt_job( key ) ) {
                --budget;
                ++started;
            }
        }
        return started;
    };

    if( lazy_omt_preload_direction_ == point_zero ) {
        auto budget = divide_round_up_size( queued, lazy_border_steps_to_cross_omt );
        budget = std::max( std::size_t{ 1 }, budget );
        TracyPlot( "Lazy Border OMT Deadline",
                   static_cast<int64_t>( lazy_border_steps_to_cross_omt ) );
        TracyPlot( "Lazy Border Credit x1000", int64_t{ 0 } );
        TracyPlot( "Lazy Border OMT Budget", static_cast<int64_t>( budget ) );
        const auto started = load_budget_matching( budget, []( const omt_key & ) {
            return true;
        } );
        if( started > 0 ) {
            run_deferred_mapgen_hooks();
            flush_deferred_zones();
            run_deferred_autonotes();
        }
        TracyPlot( "Lazy Border Z Jobs Started", static_cast<int64_t>( started ) );
        return;
    }

    const auto current_turn = to_turn<int>( calendar::turn );
    if( current_turn == lazy_omt_last_credit_turn_ ) {
        TracyPlot( "Lazy Border OMT Budget", int64_t{ 0 } );
        TracyPlot( "Lazy Border Z Jobs Started", int64_t{ 0 } );
        return;
    }
    lazy_omt_last_credit_turn_ = current_turn;

    const auto deadline = lazy_omt_focus_
                          ? turns_to_omt_edge( lazy_omt_focus_->pos, lazy_omt_preload_direction_ )
                          : lazy_border_steps_to_cross_omt;
    if( urgent == 0 ) {
        lazy_omt_budget_credit_ = 0.0;
        TracyPlot( "Lazy Border OMT Deadline", static_cast<int64_t>( deadline ) );
        TracyPlot( "Lazy Border Credit x1000", int64_t{ 0 } );
        TracyPlot( "Lazy Border OMT Budget", int64_t{ 0 } );
        TracyPlot( "Lazy Border Z Jobs Started", int64_t{ 0 } );
        return;
    }

    lazy_omt_budget_credit_ += static_cast<double>( urgent ) / static_cast<double>( deadline );

    auto budget = std::min( urgent, static_cast<std::size_t>( lazy_omt_budget_credit_ ) );
    lazy_omt_budget_credit_ -= static_cast<double>( budget );
    TracyPlot( "Lazy Border OMT Deadline", static_cast<int64_t>( deadline ) );
    TracyPlot( "Lazy Border Credit x1000",
               static_cast<int64_t>( lazy_omt_budget_credit_ * 1000.0 ) );
    TracyPlot( "Lazy Border OMT Budget", static_cast<int64_t>( budget ) );

    const auto started = load_budget_matching( budget, [&]( const omt_key & key ) {
        return lazy_omt_priority( key ) > 0;
    } );
    if( started > 0 ) {
        run_deferred_mapgen_hooks();
        flush_deferred_zones();
        run_deferred_autonotes();
    }
    TracyPlot( "Lazy Border Z Jobs Started", static_cast<int64_t>( started ) );
}

void submap_load_manager::drain_lazy_loads()
{
    ZoneScopedN( "drain_lazy_loads" );
    auto generated = false;
    std::ranges::for_each( lazy_omt_futures_, [&]( auto & entry ) {
        auto result = complete_lazy_omt_result_on_main_thread( entry.first, entry.second.get() );
        generated |= apply_lazy_omt_result( entry.first, result );
    } );
    lazy_omt_futures_.clear();
    if( generated ) {
        run_deferred_mapgen_hooks();
        flush_deferred_zones();
        run_deferred_autonotes();
    }
}

void submap_load_manager::update()
{
    ZoneScoped;

    reap_lazy_omt_jobs();

    TracyPlot( "Thread Pool Workers", static_cast<int64_t>( get_thread_pool().num_workers() ) );
    TracyPlot( "Thread Pool Queue", static_cast<int64_t>( get_thread_pool().queue_size() ) );

    // Early exit: if no request centers have changed since the last update,
    // the desired/simulated/border sets are identical — skip the expensive
    // set construction, diffing, loading, and retention work.
    {
        std::vector<std::pair<load_request_handle, tripoint>> cur_centers;
        cur_centers.reserve( requests_.size() );
        auto bubble_delta = point_zero;
        std::ranges::for_each( requests_, [&]( const auto & kv ) {
            cur_centers.emplace_back( kv.first, kv.second.center.raw() );
            const auto &req = kv.second;
            if( req.source != load_request_source::reality_bubble ) {
                return;
            }
            const auto prev = std::ranges::find_if( prev_centers_, [&]( const auto & entry ) {
                return entry.first == kv.first;
            } );
            if( prev == prev_centers_.end() ) {
                return;
            }
            bubble_delta = point{ signum( req.center.x() - prev->second.x ),
                                  signum( req.center.y() - prev->second.y ) };
        } );
        if( bubble_delta != point_zero ) {
            lazy_omt_preload_direction_ = bubble_delta;
        }
        if( cur_centers == prev_centers_ ) {
            process_lazy_border_preload();
            process_retained_omt_eviction();
            return;
        }
        prev_centers_ = std::move( cur_centers );
    }

    // Simulated set: positions that need full per-turn processing.
    auto simulated = key_set {};
    auto all_desired = key_set {};
    auto lazy_border_omts = horizontal_omt_set {};
    {
        ZoneScopedN( "slm_compute_sets" );
        simulated = compute_desired_set();
        all_desired = simulated;
        lazy_border_omts = compute_lazy_border_omts();
        add_lazy_border_into( all_desired, lazy_border_omts );
    }

    TracyPlot( "Simulated Submaps", static_cast<int64_t>( simulated.size() ) );
    TracyPlot( "Border Submaps",
               static_cast<int64_t>( all_desired.size() - simulated.size() ) );
    TracyPlot( "Total Desired Submaps", static_cast<int64_t>( all_desired.size() ) );
    erase_desired_retained_omts( all_desired );

    // ---- Synchronous loading for newly-simulated positions ----
    // new_omts is keyed by 2-D horizontal OMT position.  All z-levels for a
    // given horizontal OMT are always loaded together in the loops below.
    using horiz_omt_key = std::pair<std::string, point_abs_omt>;
    std::unordered_set<horiz_omt_key, coord_pair_hash<point_abs_omt>> new_omts;
    for( const desired_key &key : simulated ) {
        if( prev_simulated_.count( key ) == 0 ) {
            new_omts.emplace( key.first, project_to<coords::omt>( key.second ) );
        }
    }

    // Mark ALL z-levels for newly-simulated horizontal OMTs as dirty: they
    // will receive game logic and must be saved to disk when evicted.
    for( const auto &[dim_id, omt_xy] : new_omts ) {
        for( const auto z : std::views::iota( -OVERMAP_DEPTH, OVERMAP_HEIGHT + 1 ) ) {
            dirty_omts_.insert( { dim_id, tripoint_abs_omt{ omt_xy, z } } );
        }
    }

    // ---- Step 1: parallel disk preload for newly-simulated omts ----
    // preload_omt() is thread-safe (disk I/O outside submaps_mutex_; add
    // under the lock).  Running multiple omts in parallel hides disk latency.
    // Each horizontal OMT drives a z-level loop internally.
    auto resident_zlevels = std::size_t{ 0 };
    auto preloaded_zlevels = std::size_t{ 0 };
    {
        ZoneScopedN( "slm_preload_new_omts" );
        std::vector<std::future<void>> preload_futures;
        for( const auto &[dim_id, omt_xy] : new_omts ) {
            auto &mb = MAPBUFFER_REGISTRY.get( dim_id );
            for( const auto z : std::views::iota( -OVERMAP_DEPTH, OVERMAP_HEIGHT + 1 ) ) {
                const tripoint_abs_omt omt_addr{ omt_xy, z };
                const omt_key qk{ dim_id, omt_addr };
                finish_lazy_omt_job( qk );

                // Skip omts already fully resident (e.g. re-entered from
                // pending_writes cache).
                const tripoint_abs_sm sm_base = project_to<coords::sm>( omt_addr );
                const bool all_loaded =
                    mb.lookup_submap_in_memory( sm_base )
                    && mb.lookup_submap_in_memory( ( sm_base + point_east ) )
                    && mb.lookup_submap_in_memory( ( sm_base + point_south ) )
                    && mb.lookup_submap_in_memory( ( sm_base + point_south_east ) );
                if( all_loaded ) {
                    ++resident_zlevels;
                    continue;
                }
                ++preloaded_zlevels;
                preload_futures.push_back( get_thread_pool().submit_returning(
                [&mb, omt_addr]() {
                    mb.preload_omt( omt_addr );
                } ) );
            }
        }
        std::ranges::for_each( preload_futures, []( auto & f ) {
            f.get();
        } );
    } // slm_preload_new_omts
    TracyPlot( "New Sim OMT Z Resident Hits", static_cast<int64_t>( resident_zlevels ) );
    TracyPlot( "New Sim OMT Z Preload Attempts", static_cast<int64_t>( preloaded_zlevels ) );

    // Drain duplicate submaps created by concurrent preload_omt workers.
    // Must happen on the main thread (safe_reference / cata_arena not thread-safe).
    {
        auto drained_dims = std::set<std::string> {};
        std::ranges::transform( new_omts, std::inserter( drained_dims, drained_dims.end() ),
        []( const auto & qk ) { return qk.first; } );
        std::ranges::for_each( drained_dims, []( const std::string & dim_id ) {
            MAPBUFFER_REGISTRY.get( dim_id ).drain_pending_submap_destroy();
        } );
    }

    // ---- Step 2: synchronous mapgen on the main thread ----
    // generate_omt() calls tinymap::generate() which may invoke Lua mapgen.
    // Lua is not reentrant, so this must always run on the main thread.
    // Skip omts already fully resident: preload_omt loaded them from disk or
    // the pending_writes cache, so no generation is needed.
    auto generated_zlevels = std::size_t{ 0 };
    {
        ZoneScopedN( "slm_generate_new_omts" );
        for( const auto &[dim_id, omt_xy] : new_omts ) {
            auto &mb = MAPBUFFER_REGISTRY.get( dim_id );
            for( const auto z : std::views::iota( -OVERMAP_DEPTH, OVERMAP_HEIGHT + 1 ) ) {
                const tripoint_abs_omt omt_addr{ omt_xy, z };
                const tripoint_abs_sm sm_base = project_to<coords::sm>( omt_addr );
                const bool all_loaded =
                    mb.lookup_submap_in_memory( sm_base )
                    && mb.lookup_submap_in_memory( ( sm_base + point_east ) )
                    && mb.lookup_submap_in_memory( ( sm_base + point_south ) )
                    && mb.lookup_submap_in_memory( ( sm_base + point_south_east ) );
                if( !all_loaded ) {
                    ++generated_zlevels;
                    mb.generate_omt( omt_addr, {
                        .defer_postprocess_hooks = true,
                    } );
                }
            }
        }
    }
    TracyPlot( "New Sim OMT Z Generated", static_cast<int64_t>( generated_zlevels ) );

    // Drain Lua postprocess hooks queued by mapgen above.
    {
        ZoneScopedN( "slm_mapgen_hooks_sim" );
        run_deferred_mapgen_hooks();
        flush_deferred_zones();
        run_deferred_autonotes();
    }

    // ---- Listener notifications (simulated set only) ----
    // The simulated set is 2-D; fire for every z-level when a horizontal
    // position enters or leaves simulation.
    {
        ZoneScopedN( "slm_listener_notifications" );
        for( const desired_key &key : simulated ) {
            if( prev_simulated_.count( key ) == 0 ) {
                for( const auto z : std::views::iota( -OVERMAP_DEPTH, OVERMAP_HEIGHT + 1 ) ) {
                    const tripoint_abs_sm pos{ key.second, z };
                    for( submap_load_listener *listener : listeners_ ) {
                        listener->on_submap_loaded( pos, key.first );
                    }
                }
            }
        }

        for( const desired_key &key : prev_simulated_ ) {
            if( simulated.count( key ) == 0 ) {
                for( const auto z : std::views::iota( -OVERMAP_DEPTH, OVERMAP_HEIGHT + 1 ) ) {
                    const tripoint_abs_sm pos{ key.second, z };
                    for( submap_load_listener *listener : listeners_ ) {
                        listener->on_submap_unloaded( pos, key.first );
                    }
                }
            }
        }
    } // slm_listener_notifications

    // ---- Retain departed omts (full set: simulated + border) ----
    // prev_desired_ is now 2-D (horizontal SM positions).  Multiple entries
    // can map to the same horizontal OMT (up to 4: the 2×2 omt footprint).
    // omts_checked deduplicates by horizontal OMT so we retain each column
    // exactly once.  The sibling check and the z-level loop both work in
    // terms of the 2-D desired set.
    {
        ZoneScopedN( "slm_retain_departed_omts" );
        using horiz_key = std::pair<std::string, point_abs_omt>;
        std::unordered_set<horiz_key, coord_pair_hash<point_abs_omt>> omts_checked;
        for( const desired_key &key : prev_desired_ ) {
            if( all_desired.count( key ) != 0 ) {
                continue;  // still desired — skip
            }
            const point_abs_omt omt_xy = project_to<coords::omt>( key.second );
            const horiz_key ck{ key.first, omt_xy };
            if( !omts_checked.insert( ck ).second ) {
                continue;  // already handled this horizontal OMT in this cycle
            }
            // Check whether any of the 4 SM positions in this omt column
            // is still wanted (2-D check; z is irrelevant).
            const point_abs_sm sm_base = project_to<coords::sm>( omt_xy );
            bool any_still_desired = false;
            for( const point &off : { point_zero, point_south, point_east, point_south_east } ) {
                if( all_desired.count( { key.first, sm_base + off } ) ) {
                    any_still_desired = true;
                    break;
                }
            }
            if( !any_still_desired ) {
                retain_omt( { key.first, omt_xy } );
            }
        }
    }

    queue_lazy_border_omts( lazy_border_omts );
    process_lazy_border_preload();

    process_retained_omt_eviction();

    prev_simulated_ = std::move( simulated );
    prev_desired_ = std::move( all_desired );
}

bool submap_load_manager::is_requested( const std::string &dim_id,
                                        const tripoint_abs_sm &pos ) const
{
    return prev_desired_.count( { dim_id, pos.xy() } ) > 0;
}

bool submap_load_manager::is_properly_requested( const std::string &dim_id,
        const tripoint_abs_sm &pos ) const
{
    const point_abs_sm p = pos.xy();
    return std::ranges::any_of( requests_, [&]( const auto & kv ) {
        const submap_load_request &req = kv.second;
        if( req.source != load_request_source::reality_bubble ) {
            return false;
        }
        if( req.dimension_id != dim_id ) {
            return false;
        }
        const point_abs_sm c = req.center.xy();
        const int dx = std::abs( p.x() - c.x() );
        const int dy = std::abs( p.y() - c.y() );
        return dx <= req.radius && dy <= req.radius;
    } );
}

bool submap_load_manager::is_simulated( const std::string &dim_id,
                                        const tripoint_abs_sm &pos ) const
{
    if( !is_loaded( dim_id, pos ) ) { return false; }
    const point_abs_sm p = pos.xy();
    bool covered_by_lazy_only = false;
    for( const auto &[handle, req] : requests_ ) {
        if( req.dimension_id != dim_id ) {
            continue;
        }
        const point_abs_sm c = req.center.xy();
        const int dx = std::abs( p.x() - c.x() );
        const int dy = std::abs( p.y() - c.y() );
        if( !( dx <= req.radius && dy <= req.radius ) ) {
            continue;
        }
        if( req.source != load_request_source::lazy_border ) {
            return true;
        }
        covered_by_lazy_only = true;
    }
    if( covered_by_lazy_only ) {
        return false;
    }
    // No request covers this position.  Two distinct cases:
    //   • requests_ is empty  — map was loaded directly (e.g. in tests via
    //     map::load) without going through the request system.  Treat the
    //     submap as simulated so items, fields, and NPCs are processed normally.
    //   • requests_ is non-empty — the submap was loaded as a omt-alignment
    //     overflow beyond the lazy-border zone (odd bubble size forces an extra
    //     row/column of submaps to be resident).  It should not be simulated.
    return requests_.empty();
}

bool submap_load_manager::is_loaded( const std::string &dim_id,
                                     const tripoint_abs_sm &pos ) const
{
    return MAPBUFFER_REGISTRY.get( dim_id ).lookup_submap_in_memory( pos ) != nullptr;
}

std::vector<std::string> submap_load_manager::active_dimensions() const
{
    std::set<std::string> dims;
    for( const auto &kv : requests_ ) {
        dims.insert( kv.second.dimension_id );
    }
    return { dims.begin(), dims.end() };
}

auto submap_load_manager::non_bubble_requests() const -> std::vector<submap_load_request>
{
    auto is_non_bubble = []( const auto & kv ) {
        return kv.second.source != load_request_source::reality_bubble
               && kv.second.source != load_request_source::lazy_border;
    };
    auto to_request = []( const auto & kv ) -> const submap_load_request & {
        return kv.second;
    };
    auto view = requests_ | std::views::filter( is_non_bubble )
                | std::views::transform( to_request );
    return { view.begin(), view.end() };
}

auto submap_load_manager::is_fully_drained() const noexcept -> bool
{
    return lazy_omt_futures_.empty();
}

void submap_load_manager::flush_prev_desired()
{
    assert( is_fully_drained() );
    prev_desired_.clear();
    prev_simulated_.clear();
    prev_centers_.clear();
    retained_omts_.clear();
    retained_omt_index_.clear();
    lazy_omt_jobs_.clear();
    lazy_omt_job_index_.clear();
    lazy_omt_futures_.clear();
    lazy_omt_preload_direction_ = point_zero;
    lazy_omt_focus_.reset();
    lazy_omt_budget_credit_ = 0.0;
    lazy_omt_last_credit_turn_ = -1;
    dirty_omts_.clear();
}

void submap_load_manager::add_listener( submap_load_listener *listener )
{
    if( std::find( listeners_.begin(), listeners_.end(), listener ) == listeners_.end() ) {
        listeners_.push_back( listener );
    }
}

void submap_load_manager::remove_listener( submap_load_listener *listener )
{
    listeners_.erase( std::remove( listeners_.begin(), listeners_.end(), listener ),
                      listeners_.end() );
}
