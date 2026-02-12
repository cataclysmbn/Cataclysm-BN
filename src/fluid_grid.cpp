#include "fluid_grid.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <memory>
#include <optional>
#include <queue>
#include <ranges>

#include "calendar.h"
#include "coordinate_conversions.h"
#include "coordinates.h"
#include "cuboid_rectangle.h"
#include "debug.h"
#include "game_constants.h"
#include "item.h"
#include "mapbuffer.h"
#include "mapdata.h"
#include "memory_fast.h"
#include "map.h"
#include "messages.h"
#include "overmap.h"
#include "overmapbuffer.h"
#include "output.h"
#include "point.h"
#include "rng.h"
#include "submap.h"
#include "translations.h"
#include "weather.h"

namespace
{

using connection_store = std::map<point_abs_om, fluid_grid::connection_map>;
using storage_store =
    std::map<point_abs_om, std::map<tripoint_om_omt, fluid_grid::liquid_storage_state>>;
using grid_member_set = std::set<tripoint_om_omt>;
using grid_member_ptr = shared_ptr_fast<grid_member_set>;
using grid_member_map = std::map<tripoint_om_omt, grid_member_ptr>;
using grid_member_cache_store = std::map<point_abs_om, grid_member_map>;
struct submap_tank_cache_entry {
    units::volume capacity = 0_ml;
    int tank_count = 0;
};
using submap_tank_cache_store = std::map<tripoint_abs_sm, submap_tank_cache_entry>;
using transformer_cache_store = std::map<tripoint_abs_sm, bool>;
static const itype_id itype_water( "water" );
static const itype_id itype_water_clean( "water_clean" );
static constexpr double mm2_per_m2 = 1000.0 * 1000.0;

auto tank_capacity_for_furn( const furn_t &furn ) -> std::optional<units::volume>
{
    if( !furn.fluid_grid ) {
        return std::nullopt;
    }
    const auto &fluid_grid = *furn.fluid_grid;
    if( fluid_grid.role != fluid_grid_role::tank ) {
        return std::nullopt;
    }
    if( fluid_grid.capacity ) {
        return fluid_grid.capacity;
    }
    if( fluid_grid.use_keg_capacity ) {
        return furn.keg_capacity;
    }
    return std::nullopt;
}

auto fluid_grid_store() -> connection_store & // *NOPAD*
{
    static auto store = connection_store{};
    return store;
}

auto empty_connections() -> const fluid_grid::connection_map & // *NOPAD*
{
    static const auto empty = fluid_grid::connection_map{};
    return empty;
}

auto fluid_storage_store() -> storage_store & // *NOPAD*
{
    static auto store = storage_store{};
    return store;
}

auto empty_storage() -> const std::map<tripoint_om_omt, fluid_grid::liquid_storage_state>
& // *NOPAD*
{
    static const auto empty = std::map<tripoint_om_omt, fluid_grid::liquid_storage_state> {};
    return empty;
}

auto empty_bitset() -> const fluid_grid::connection_bitset & // *NOPAD*
{
    static const auto empty = fluid_grid::connection_bitset{};
    return empty;
}

auto grid_members_cache() -> grid_member_cache_store & // *NOPAD*
{
    static auto cache = grid_member_cache_store{};
    return cache;
}

auto submap_tank_cache() -> submap_tank_cache_store & // *NOPAD*
{
    static auto cache = submap_tank_cache_store{};
    return cache;
}

auto transformer_cache() -> transformer_cache_store & // *NOPAD*
{
    static auto cache = transformer_cache_store{};
    return cache;
}

auto transformer_last_run_at( const tripoint_abs_ms &p ) -> time_point
{
    auto target_sm = tripoint_abs_sm{};
    auto target_pos = point_sm_ms{};
    std::tie( target_sm, target_pos ) = project_remain<coords::sm>( p );
    auto *target_submap = MAPBUFFER.lookup_submap( target_sm );
    if( target_submap == nullptr ) {
        return calendar::start_of_cataclysm;
    }

    const auto iter = target_submap->transformer_last_run.find( target_pos );
    if( iter == target_submap->transformer_last_run.end() ) {
        return calendar::start_of_cataclysm;
    }

    return iter->second;
}

auto set_transformer_last_run_at( const tripoint_abs_ms &p, time_point t ) -> void
{
    auto target_sm = tripoint_abs_sm{};
    auto target_pos = point_sm_ms{};
    std::tie( target_sm, target_pos ) = project_remain<coords::sm>( p );
    auto *target_submap = MAPBUFFER.lookup_submap( target_sm );
    if( target_submap == nullptr ) {
        return;
    }

    target_submap->transformer_last_run[target_pos] = t;
}

auto clear_transformer_last_run_at( const tripoint_abs_ms &p ) -> void
{
    auto target_sm = tripoint_abs_sm{};
    auto target_pos = point_sm_ms{};
    std::tie( target_sm, target_pos ) = project_remain<coords::sm>( p );
    auto *target_submap = MAPBUFFER.lookup_submap( target_sm );
    if( target_submap == nullptr ) {
        return;
    }

    target_submap->transformer_last_run.erase( target_pos );
}

auto ticks_between( const time_point &from, const time_point &to,
                    const time_duration &tick_length ) -> int
{
    if( tick_length <= 0_turns ) {
        return 0;
    }
    return ( to_turn<int>( to ) / to_turns<int>( tick_length ) ) -
           ( to_turn<int>( from ) / to_turns<int>( tick_length ) );
}

auto volume_from_charges( const itype_id &liquid_type, int charges ) -> units::volume
{
    if( charges <= 0 ) {
        return 0_ml;
    }
    auto liquid = item::spawn( liquid_type, calendar::turn, charges );
    return liquid->volume();
}

auto charges_from_volume( const itype_id &liquid_type, units::volume volume ) -> int
{
    if( volume <= 0_ml ) {
        return 0;
    }
    auto liquid = item::spawn( liquid_type, calendar::turn );
    return liquid->charges_per_volume( volume );
}

auto batches_for_inputs( const std::vector<fluid_grid_transform_io> &inputs,
                         const std::map<itype_id, units::volume> &available ) -> double
{
    if( inputs.empty() ) {
        return 0.0;
    }

    auto max_batches = std::numeric_limits<double>::max();
    std::ranges::for_each( inputs, [&]( const fluid_grid_transform_io & io ) {
        const auto amount_ml = units::to_milliliter<int>( io.amount );
        if( amount_ml <= 0 ) {
            max_batches = 0.0;
            return;
        }
        const auto iter = available.find( io.liquid );
        if( iter == available.end() ) {
            max_batches = 0.0;
            return;
        }
        const auto available_ml = units::to_milliliter<double>( iter->second );
        const auto required_ml = units::to_milliliter<double>( io.amount );
        if( available_ml <= 0.0 || required_ml <= 0.0 ) {
            max_batches = 0.0;
            return;
        }
        max_batches = std::min( max_batches, available_ml / required_ml );
    } );

    if( max_batches == std::numeric_limits<double>::max() ) {
        return 0.0;
    }
    return std::max( max_batches, 0.0 );
}

auto is_outdoors_at( const tripoint_abs_ms &p ) -> bool
{
    return get_map().is_outside( get_map().getlocal( p.raw() ) );
}

auto rain_charges_for( double collector_area_m2, const weather_sum &weather ) -> int
{
    if( collector_area_m2 <= 0.0 ) {
        return 0;
    }

    if( weather.rain_amount <= 0 ) {
        return 0;
    }

    const auto surface_area_mm2 = collector_area_m2 * mm2_per_m2;
    const auto charges_per_turn =
        funnel_charges_per_turn( surface_area_mm2, static_cast<double>( weather.rain_amount ) );
    if( charges_per_turn <= 0.0 ) {
        return 0;
    }

    return roll_remainder( charges_per_turn );
}

struct transformer_instance {
    tripoint_abs_ms pos = tripoint_abs_ms( tripoint_zero );
    const fluid_grid_transformer_config *config = nullptr;
    fluid_grid_role role = fluid_grid_role::transformer;
};

auto collect_transformers( const std::set<tripoint_abs_omt> &grid,
                           mapbuffer &mb ) -> std::vector<transformer_instance>
{
    auto submap_positions = std::set<tripoint_abs_sm> {};
    std::ranges::for_each( grid, [&]( const tripoint_abs_omt & omp ) {
        const auto base = project_to<coords::sm>( omp );
        submap_positions.emplace( base + point_zero );
        submap_positions.emplace( base + point_east );
        submap_positions.emplace( base + point_south );
        submap_positions.emplace( base + point_south_east );
    } );

    auto transformers = std::vector<transformer_instance> {};
    std::ranges::for_each( submap_positions, [&]( const tripoint_abs_sm & sm_coord ) {
        auto *sm = mb.lookup_submap( sm_coord );
        if( sm == nullptr ) {
            return;
        }
        std::ranges::for_each( std::views::iota( 0, SEEX ), [&]( int x ) {
            std::ranges::for_each( std::views::iota( 0, SEEY ), [&]( int y ) {
                const auto pos = point( x, y );
                const auto &furn = sm->get_furn( pos ).obj();
                if( !furn.fluid_grid ) {
                    return;
                }
                if( furn.fluid_grid->role != fluid_grid_role::transformer &&
                    furn.fluid_grid->role != fluid_grid_role::rain_collector ) {
                    return;
                }
                if( !furn.fluid_grid->transformer ) {
                    return;
                }
                const auto abs_pos = project_combine( sm_coord, point_sm_ms( pos ) );
                transformers.push_back( transformer_instance{
                    .pos = abs_pos,
                    .config = &*furn.fluid_grid->transformer,
                    .role = furn.fluid_grid->role
                } );
            } );
        } );
    } );

    return transformers;
}

auto tank_capacity_at( mapbuffer &mb, const tripoint_abs_ms &p ) -> std::optional<units::volume>
{
    auto target_sm = tripoint_abs_sm{};
    auto target_pos = point_sm_ms{};
    std::tie( target_sm, target_pos ) = project_remain<coords::sm>( p );
    auto *target_submap = mb.lookup_submap( target_sm );
    if( target_submap == nullptr ) {
        return std::nullopt;
    }

    const auto &furn = target_submap->get_furn( target_pos.raw() ).obj();
    return tank_capacity_for_furn( furn );
}

auto has_transformer_at( mapbuffer &mb, const tripoint_abs_ms &p ) -> bool
{
    auto target_sm = tripoint_abs_sm{};
    auto target_pos = point_sm_ms{};
    std::tie( target_sm, target_pos ) = project_remain<coords::sm>( p );
    auto *target_submap = mb.lookup_submap( target_sm );
    if( target_submap == nullptr ) {
        return false;
    }

    const auto &furn = target_submap->get_furn( target_pos.raw() ).obj();
    if( !furn.fluid_grid ) {
        return false;
    }
    if( furn.fluid_grid->role != fluid_grid_role::transformer &&
        furn.fluid_grid->role != fluid_grid_role::rain_collector ) {
        return false;
    }
    return static_cast<bool>( furn.fluid_grid->transformer );
}

auto is_supported_liquid( const itype_id &liquid_type ) -> bool
{
    return liquid_type == itype_water || liquid_type == itype_water_clean;
}

auto ordered_liquid_types( const fluid_grid::liquid_storage_state &state ) -> std::vector<itype_id>
{
    auto ordered = std::vector<itype_id> { itype_water, itype_water_clean };
    std::ranges::for_each( state.stored_by_type | std::views::keys,
    [&]( const itype_id & liquid_type ) {
        if( std::ranges::find( ordered, liquid_type ) == ordered.end() ) {
            ordered.push_back( liquid_type );
        }
    } );
    return ordered;
}

auto normalize_water_storage( fluid_grid::liquid_storage_state &state,
                              bool allow_mixed_water ) -> void
{
    if( allow_mixed_water ) {
        return;
    }
    auto clean_iter = state.stored_by_type.find( itype_water_clean );
    auto dirty_iter = state.stored_by_type.find( itype_water );
    if( clean_iter != state.stored_by_type.end() && dirty_iter != state.stored_by_type.end() ) {
        dirty_iter->second += clean_iter->second;
        state.stored_by_type.erase( clean_iter );
    }
    clean_iter = state.stored_by_type.find( itype_water_clean );
    if( clean_iter != state.stored_by_type.end() && clean_iter->second <= 0_ml ) {
        state.stored_by_type.erase( clean_iter );
    }
    dirty_iter = state.stored_by_type.find( itype_water );
    if( dirty_iter != state.stored_by_type.end() && dirty_iter->second <= 0_ml ) {
        state.stored_by_type.erase( dirty_iter );
    }
}

auto taint_clean_water( fluid_grid::liquid_storage_state &state,
                        bool allow_mixed_water ) -> void
{
    if( allow_mixed_water ) {
        return;
    }
    auto clean_iter = state.stored_by_type.find( itype_water_clean );
    if( clean_iter == state.stored_by_type.end() ) {
        return;
    }
    state.stored_by_type[itype_water] += clean_iter->second;
    state.stored_by_type.erase( clean_iter );
}

auto cleanup_zero_storage( fluid_grid::liquid_storage_state &state ) -> void
{
    auto to_erase = std::vector<itype_id> {};
    std::ranges::for_each( state.stored_by_type, [&]( const auto & entry ) {
        if( entry.second <= 0_ml ) {
            to_erase.push_back( entry.first );
        }
    } );
    std::ranges::for_each( to_erase, [&]( const itype_id & liquid_type ) {
        state.stored_by_type.erase( liquid_type );
    } );
}

auto stored_types_in_order( const fluid_grid::liquid_storage_state &state ) -> std::vector<itype_id>
{
    auto present = std::vector<itype_id> {};
    const auto ordered = ordered_liquid_types( state );
    present.reserve( ordered.size() );
    std::ranges::for_each( ordered, [&]( const itype_id & liquid_type ) {
        const auto iter = state.stored_by_type.find( liquid_type );
        if( iter == state.stored_by_type.end() ) {
            return;
        }
        if( iter->second <= 0_ml ) {
            return;
        }
        present.push_back( liquid_type );
    } );
    return present;
}

auto enforce_tank_type_limit( fluid_grid::liquid_storage_state &state,
                              int tank_count ) -> void
{
    cleanup_zero_storage( state );
    if( tank_count <= 0 ) {
        state.stored_by_type.clear();
        return;
    }
    auto ordered_present = stored_types_in_order( state );
    if( ordered_present.size() <= static_cast<size_t>( tank_count ) ) {
        return;
    }
    const auto primary = ordered_present.front();
    const auto keep_count = static_cast<size_t>( std::max( tank_count, 1 ) );
    const auto merge_view = ordered_present | std::views::drop( keep_count );
    std::ranges::for_each( merge_view, [&]( const itype_id & liquid_type ) {
        const auto iter = state.stored_by_type.find( liquid_type );
        if( iter == state.stored_by_type.end() ) {
            return;
        }
        if( iter->second <= 0_ml ) {
            state.stored_by_type.erase( iter );
            return;
        }
        state.stored_by_type[primary] += iter->second;
        state.stored_by_type.erase( iter );
    } );
}

auto reduce_storage( fluid_grid::liquid_storage_state &state,
                     units::volume volume ) -> fluid_grid::liquid_storage_state
{
    auto remaining = volume;
    auto removed = fluid_grid::liquid_storage_state{};

    if( remaining <= 0_ml ) {
        return removed;
    }

    const auto ordered = ordered_liquid_types( state );
    std::ranges::for_each( ordered, [&]( const itype_id & liquid_type ) {
        if( remaining <= 0_ml ) {
            return;
        }
        const auto iter = state.stored_by_type.find( liquid_type );
        if( iter == state.stored_by_type.end() ) {
            return;
        }
        const auto available = iter->second;
        if( available <= 0_ml ) {
            state.stored_by_type.erase( iter );
            return;
        }
        const auto removed_volume = std::min( available, remaining );
        iter->second -= removed_volume;
        removed.stored_by_type[liquid_type] += removed_volume;
        remaining -= removed_volume;
        if( iter->second <= 0_ml ) {
            state.stored_by_type.erase( iter );
        }
    } );

    return removed;
}

auto merge_storage_states( const fluid_grid::liquid_storage_state &lhs,
                           const fluid_grid::liquid_storage_state &rhs ) ->
fluid_grid::liquid_storage_state
{
    auto merged = fluid_grid::liquid_storage_state{
        .stored_by_type = lhs.stored_by_type,
        .capacity = lhs.capacity + rhs.capacity
    };
    std::ranges::for_each( rhs.stored_by_type, [&]( const auto & entry ) {
        merged.stored_by_type[entry.first] += entry.second;
    } );
    return merged;
}

auto anchor_for_grid( const std::set<tripoint_abs_omt> &grid ) -> tripoint_abs_omt
{
    if( grid.empty() ) {
        return tripoint_abs_omt{ tripoint_zero };
    }

    return *std::ranges::min_element( grid, []( const tripoint_abs_omt & lhs,
    const tripoint_abs_omt & rhs ) {
        return lhs.raw() < rhs.raw();
    } );
}

auto collect_storage_for_grid( const tripoint_abs_omt &anchor_abs,
                               const std::set<tripoint_abs_omt> &grid ) -> fluid_grid::liquid_storage_state
{
    auto total = fluid_grid::liquid_storage_state{};
    if( grid.empty() ) {
        return total;
    }

    auto omc = overmap_buffer.get_om_global( anchor_abs );
    auto &storage = fluid_grid::storage_for( *omc.om );
    auto to_erase = std::vector<tripoint_om_omt> {};

    std::ranges::for_each( storage, [&]( const auto & entry ) {
        const auto abs_pos = project_combine( omc.om->pos(), entry.first );
        if( grid.contains( abs_pos ) ) {
            std::ranges::for_each( entry.second.stored_by_type, [&]( const auto & liquid_entry ) {
                total.stored_by_type[liquid_entry.first] += liquid_entry.second;
            } );
            to_erase.push_back( entry.first );
        }
    } );

    std::ranges::for_each( to_erase, [&]( const tripoint_om_omt & entry ) {
        storage.erase( entry );
    } );

    return total;
}

auto invalidate_grid_members_cache( const point_abs_om &om_pos ) -> void
{
    grid_members_cache().erase( om_pos );
}

auto invalidate_grid_members_cache_at( const tripoint_abs_omt &p ) -> void
{
    const auto omc = overmap_buffer.get_om_global( p );
    invalidate_grid_members_cache( omc.om->pos() );
}

auto invalidate_submap_cache_at( const tripoint_abs_sm &p ) -> void
{
    submap_tank_cache().erase( p );
}

auto invalidate_transformer_cache_at( const tripoint_abs_sm &p ) -> void
{
    transformer_cache().erase( p );
}

auto submap_has_transformer( const tripoint_abs_sm &sm_coord, mapbuffer &mb ) -> bool
{
    auto &cache = transformer_cache();
    const auto iter = cache.find( sm_coord );
    if( iter != cache.end() ) {
        return iter->second;
    }

    auto *sm = mb.lookup_submap( sm_coord );
    if( sm == nullptr ) {
        cache.emplace( sm_coord, false );
        return false;
    }

    auto found = false;
    std::ranges::for_each( std::views::iota( 0, SEEX ), [&]( int x ) {
        if( found ) {
            return;
        }
        std::ranges::for_each( std::views::iota( 0, SEEY ), [&]( int y ) {
            if( found ) {
                return;
            }
            const auto pos = point( x, y );
            const auto &furn = sm->get_furn( pos ).obj();
            if( !furn.fluid_grid ) {
                return;
            }
            if( furn.fluid_grid->role != fluid_grid_role::transformer &&
                furn.fluid_grid->role != fluid_grid_role::rain_collector ) {
                return;
            }
            found = true;
        } );
    } );

    cache.emplace( sm_coord, found );
    return found;
}

auto storage_state_for_grid( const std::set<tripoint_abs_omt> &grid ) ->
fluid_grid::liquid_storage_state
{
    if( grid.empty() ) {
        return {};
    }
    const auto anchor_abs = anchor_for_grid( grid );
    const auto omc = overmap_buffer.get_om_global( anchor_abs );
    const auto &storage = fluid_grid::storage_for( *omc.om );
    const auto iter = storage.find( omc.local );
    if( iter == storage.end() ) {
        return {};
    }
    return iter->second;
}

auto connection_bitset_at( const overmap &om,
                           const tripoint_om_omt &p ) -> const fluid_grid::connection_bitset & // *NOPAD*
{
    const auto &connections = fluid_grid::connections_for( om );
    const auto iter = connections.find( p );
    if( iter == connections.end() ) {
        return empty_bitset();
    }
    return iter->second;
}

auto build_grid_members( const overmap &om, const tripoint_om_omt &p ) -> grid_member_set
{
    auto result = grid_member_set{};
    auto open = std::queue<tripoint_om_omt> {};
    open.emplace( p );

    while( !open.empty() ) {
        const auto elem = open.front();
        result.emplace( elem );
        const auto &connections_bitset = connection_bitset_at( om, elem );
        std::ranges::for_each( std::views::iota( size_t{ 0 }, six_cardinal_directions.size() ),
        [&]( size_t i ) {
            if( connections_bitset.test( i ) ) {
                const auto other = elem + six_cardinal_directions[i];
                if( !result.contains( other ) ) {
                    open.emplace( other );
                }
            }
        } );
        open.pop();
    }

    return result;
}

auto grid_members_for( const tripoint_abs_omt &p ) -> const grid_member_set & // *NOPAD*
{
    const auto omc = overmap_buffer.get_om_global( p );
    auto &cache = grid_members_cache()[omc.om->pos()];
    const auto iter = cache.find( omc.local );
    if( iter != cache.end() ) {
        return *iter->second;
    }

    auto members = build_grid_members( *omc.om, omc.local );
    auto members_ptr = make_shared_fast<grid_member_set>( std::move( members ) );
    std::ranges::for_each( *members_ptr, [&]( const tripoint_om_omt & entry ) {
        cache[entry] = members_ptr;
    } );

    return *members_ptr;
}

auto submap_tank_cache_at( const tripoint_abs_sm &sm_coord,
                           mapbuffer &mb ) -> submap_tank_cache_entry
{
    auto &cache = submap_tank_cache();
    const auto iter = cache.find( sm_coord );
    if( iter != cache.end() ) {
        return iter->second;
    }

    auto *sm = mb.lookup_submap( sm_coord );
    if( sm == nullptr ) {
        return submap_tank_cache_entry{};
    }

    auto entry = submap_tank_cache_entry{};
    std::ranges::for_each( std::views::iota( 0, SEEX ), [&]( int x ) {
        std::ranges::for_each( std::views::iota( 0, SEEY ), [&]( int y ) {
            const auto pos = point( x, y );
            const auto &furn = sm->get_furn( pos ).obj();
            const auto tank_capacity = tank_capacity_for_furn( furn );
            if( tank_capacity ) {
                entry.capacity += *tank_capacity;
                ++entry.tank_count;
            }
        } );
    } );

    cache.emplace( sm_coord, entry );
    return entry;
}

auto tank_count_for_grid( const std::vector<tripoint_abs_sm> &submap_coords,
                          mapbuffer &mb ) -> int
{
    auto total = 0;
    std::ranges::for_each( submap_coords, [&]( const tripoint_abs_sm & sm_coord ) {
        total += submap_tank_cache_at( sm_coord, mb ).tank_count;
    } );
    return total;
}

auto tank_count_for_grid( const std::set<tripoint_abs_omt> &grid,
                          mapbuffer &mb ) -> int
{
    auto total = 0;
    std::ranges::for_each( grid, [&]( const tripoint_abs_omt & omp ) {
        const auto sm_coord = project_to<coords::sm>( omp );
        total += submap_tank_cache_at( sm_coord + point_zero, mb ).tank_count;
        total += submap_tank_cache_at( sm_coord + point_east, mb ).tank_count;
        total += submap_tank_cache_at( sm_coord + point_south, mb ).tank_count;
        total += submap_tank_cache_at( sm_coord + point_south_east, mb ).tank_count;
    } );
    return total;
}

auto tank_count_for_grid_excluding( const std::set<tripoint_abs_omt> &grid,
                                    mapbuffer &mb,
                                    const std::optional<tripoint_abs_ms> &exclude ) -> int
{
    auto total = tank_count_for_grid( grid, mb );
    if( !exclude ) {
        return total;
    }
    if( tank_capacity_at( mb, *exclude ) ) {
        return std::max( total - 1, 0 );
    }
    return total;
}

auto calculate_capacity_for_grid( const std::set<tripoint_abs_omt> &grid,
                                  mapbuffer &mb ) -> units::volume
{
    auto submap_positions = std::set<tripoint_abs_sm> {};
    std::ranges::for_each( grid, [&]( const tripoint_abs_omt & omp ) {
        const auto base = project_to<coords::sm>( omp );
        submap_positions.emplace( base + point_zero );
        submap_positions.emplace( base + point_east );
        submap_positions.emplace( base + point_south );
        submap_positions.emplace( base + point_south_east );
    } );

    auto total = 0_ml;
    std::ranges::for_each( submap_positions, [&]( const tripoint_abs_sm & sm_coord ) {
        total += submap_tank_cache_at( sm_coord, mb ).capacity;
    } );

    return total;
}

struct split_storage_result {
    fluid_grid::liquid_storage_state lhs;
    fluid_grid::liquid_storage_state rhs;
};

auto split_storage_state( const fluid_grid::liquid_storage_state &state,
                          units::volume lhs_capacity,
                          units::volume rhs_capacity ) -> split_storage_result
{
    auto lhs_state = fluid_grid::liquid_storage_state{
        .stored_by_type = {},
        .capacity = lhs_capacity
    };
    auto rhs_state = fluid_grid::liquid_storage_state{
        .stored_by_type = {},
        .capacity = rhs_capacity
    };

    const auto total_capacity = lhs_capacity + rhs_capacity;
    if( total_capacity <= 0_ml ) {
        return { .lhs = lhs_state, .rhs = rhs_state };
    }

    const auto total_capacity_ml = units::to_milliliter<int>( total_capacity );
    const auto lhs_capacity_ml = units::to_milliliter<int>( lhs_capacity );
    const auto ratio = static_cast<double>( lhs_capacity_ml ) / total_capacity_ml;

    std::ranges::for_each( state.stored_by_type, [&]( const auto & entry ) {
        const auto liquid_ml = units::to_milliliter<int>( entry.second );
        const auto lhs_liquid_ml = static_cast<int>( std::round( liquid_ml * ratio ) );

        lhs_state.stored_by_type[entry.first] = units::from_milliliter( lhs_liquid_ml );
        rhs_state.stored_by_type[entry.first] =
            entry.second - lhs_state.stored_by_type[entry.first];

        if( lhs_state.stored_by_type[entry.first] <= 0_ml ) {
            lhs_state.stored_by_type.erase( entry.first );
        }
        if( rhs_state.stored_by_type[entry.first] <= 0_ml ) {
            rhs_state.stored_by_type.erase( entry.first );
        }
    } );

    return { .lhs = lhs_state, .rhs = rhs_state };
}

auto connection_bitset_at( const tripoint_abs_omt &p ) -> const fluid_grid::connection_bitset
& // *NOPAD*
{
    const auto omc = overmap_buffer.get_om_global( p );
    const auto &connections = fluid_grid::connections_for( *omc.om );
    const auto iter = connections.find( omc.local );
    if( iter == connections.end() ) {
        return empty_bitset();
    }
    return iter->second;
}

auto connection_bitset_at( overmap &om,
                           const tripoint_om_omt &p ) -> fluid_grid::connection_bitset & // *NOPAD*
{
    auto &connections = fluid_grid::connections_for( om );
    return connections[p];
}

class fluid_storage_grid
{
    private:
        std::vector<tripoint_abs_sm> submap_coords;
        tripoint_abs_omt anchor_abs;
        fluid_grid::liquid_storage_state state;
        mutable std::optional<fluid_grid::liquid_storage_stats> cached_stats;

        mapbuffer &mb;

    public:
        struct fluid_storage_grid_options {
            const std::vector<tripoint_abs_sm> *submap_coords = nullptr;
            tripoint_abs_omt anchor = tripoint_abs_omt{ tripoint_zero };
            fluid_grid::liquid_storage_state initial_state;
            mapbuffer *buffer = nullptr;
        };

        explicit fluid_storage_grid( const fluid_storage_grid_options &opts ) :
            submap_coords( *opts.submap_coords ),
            anchor_abs( opts.anchor ),
            state( opts.initial_state ),
            mb( *opts.buffer ) {
            state.capacity = calculate_capacity();
            if( state.stored_total() > state.capacity ) {
                reduce_storage( state, state.stored_total() - state.capacity );
            }
            sync_storage();
        }

        auto empty() const -> bool {
            return state.capacity <= 0_ml;
        }

        auto invalidate() -> void {
            cached_stats.reset();
        }

        auto get_stats() const -> fluid_grid::liquid_storage_stats {
            if( cached_stats ) {
                return *cached_stats;
            }

            auto stats = fluid_grid::liquid_storage_stats{
                .stored = std::min( state.stored_total(), state.capacity ),
                .capacity = state.capacity,
                .stored_by_type = state.stored_by_type
            };

            cached_stats = stats;
            return stats;
        }

        auto total_charges( const itype_id &liquid_type ) const -> int {
            return charges_from_volume( liquid_type, state.stored_for( liquid_type ) );
        }

        auto allow_mixed_water() const -> bool {
            return tank_count_for_grid( submap_coords, mb ) > 1;
        }

        auto drain_charges( const itype_id &liquid_type, int charges ) -> int {
            if( charges <= 0 ) {
                return 0;
            }
            const auto available = total_charges( liquid_type );
            const auto used = std::min( charges, available );
            if( used <= 0 ) {
                return 0;
            }

            const auto used_volume = volume_from_charges( liquid_type, used );
            const auto iter = state.stored_by_type.find( liquid_type );
            if( iter != state.stored_by_type.end() ) {
                iter->second -= std::min( iter->second, used_volume );
                if( iter->second <= 0_ml ) {
                    state.stored_by_type.erase( iter );
                }
            }

            cached_stats.reset();
            sync_storage();
            return used;
        }

        auto add_charges( const itype_id &liquid_type, int charges ) -> int {
            if( charges <= 0 ) {
                return 0;
            }
            if( !is_supported_liquid( liquid_type ) ) {
                return 0;
            }

            const auto allow_mixed = allow_mixed_water();
            normalize_water_storage( state, allow_mixed );
            if( liquid_type == itype_water ) {
                taint_clean_water( state, allow_mixed );
            }

            const auto available_volume = state.capacity - state.stored_total();
            if( available_volume <= 0_ml ) {
                return 0;
            }

            const auto max_charges = charges_from_volume( liquid_type, available_volume );
            const auto added = std::min( charges, max_charges );
            if( added <= 0 ) {
                return 0;
            }

            const auto added_volume = volume_from_charges( liquid_type, added );
            if( liquid_type == itype_water_clean ) {
                const auto dirty_iter = state.stored_by_type.find( itype_water );
                if( dirty_iter != state.stored_by_type.end() ) {
                    dirty_iter->second += added_volume;
                } else {
                    state.stored_by_type[itype_water_clean] += added_volume;
                }
            } else {
                state.stored_by_type[itype_water] += added_volume;
            }
            cached_stats.reset();
            sync_storage();
            return added;
        }

        auto get_state() const -> fluid_grid::liquid_storage_state {
            return state;
        }

        auto set_state( const fluid_grid::liquid_storage_state &new_state ) -> void {
            state = new_state;
            normalize_water_storage( state, allow_mixed_water() );
            cached_stats.reset();
            sync_storage();
        }

        auto set_capacity( units::volume capacity ) -> void {
            state.capacity = std::max( capacity, 0_ml );
            if( state.stored_total() > state.capacity ) {
                reduce_storage( state, state.stored_total() - state.capacity );
            }
            cached_stats.reset();
            sync_storage();
        }

    private:
        auto calculate_capacity() const -> units::volume {
            auto total = 0_ml;
            std::ranges::for_each( submap_coords, [&]( const tripoint_abs_sm & sm_coord ) {
                total += submap_tank_cache_at( sm_coord, mb ).capacity;
            } );
            return total;
        }

        auto sync_storage() -> void {
            auto omc = overmap_buffer.get_om_global( anchor_abs );
            auto &storage = fluid_grid::storage_for( *omc.om );
            storage[omc.local] = state;
        }
};

class fluid_grid_tracker
{
    private:
        std::map<tripoint_abs_sm, shared_ptr_fast<fluid_storage_grid>> parent_storage_grids;
        mapbuffer &mb;
        std::optional<half_open_rectangle<point_abs_sm>> bounds;
        std::set<tripoint_abs_omt> grids_requiring_updates;

        auto is_within_bounds( const tripoint_abs_sm &sm ) const -> bool {
            return bounds && bounds->contains( sm.xy() );
        }

        auto rebuild_transformer_grids() -> void {
            grids_requiring_updates.clear();
            if( !bounds ) {
                return;
            }
            std::ranges::for_each( mb, [&]( const auto & entry ) {
                const auto abs_sm = tripoint_abs_sm( entry.first );
                if( !is_within_bounds( abs_sm ) ) {
                    return;
                }
                if( !submap_has_transformer( abs_sm, mb ) ) {
                    return;
                }
                grids_requiring_updates.insert( project_to<coords::omt>( abs_sm ) );
            } );
        }

        auto update_transformer_grid_at( const tripoint_abs_omt &omt_pos ) -> void {
            if( !bounds ) {
                return;
            }
            const auto base = project_to<coords::sm>( omt_pos );
            const auto submaps = std::array<tripoint_abs_sm, 4> {
                base + point_zero,
                base + point_east,
                base + point_south,
                base + point_south_east
            };
            const auto has_transformer = std::ranges::any_of( submaps,
            [&]( const tripoint_abs_sm & sm ) {
                return is_within_bounds( sm ) && submap_has_transformer( sm, mb );
            } );
            if( has_transformer ) {
                grids_requiring_updates.insert( omt_pos );
            } else {
                grids_requiring_updates.erase( omt_pos );
            }
        }

        auto make_storage_grid_at( const tripoint_abs_sm &sm_pos ) -> fluid_storage_grid& { // *NOPAD*
            const auto overmap_positions = fluid_grid::grid_at( project_to<coords::omt>( sm_pos ) );
            auto submap_positions = std::vector<tripoint_abs_sm> {};
            submap_positions.reserve( overmap_positions.size() * 4 );

            std::ranges::for_each( overmap_positions, [&]( const tripoint_abs_omt & omp ) {
                const auto base = project_to<coords::sm>( omp );
                submap_positions.emplace_back( base + point_zero );
                submap_positions.emplace_back( base + point_east );
                submap_positions.emplace_back( base + point_south );
                submap_positions.emplace_back( base + point_south_east );
            } );

            if( submap_positions.empty() ) {
                static const auto empty_submaps = std::vector<tripoint_abs_sm> {};
                static const auto empty_options = fluid_storage_grid::fluid_storage_grid_options{
                    .submap_coords = &empty_submaps,
                    .anchor = tripoint_abs_omt{ tripoint_zero },
                    .initial_state = fluid_grid::liquid_storage_state{},
                    .buffer = &MAPBUFFER
                };
                static auto empty_storage_grid = fluid_storage_grid( empty_options );
                return empty_storage_grid;
            }

            const auto anchor_abs = anchor_for_grid( overmap_positions );
            const auto initial_state = collect_storage_for_grid( anchor_abs, overmap_positions );
            auto options = fluid_storage_grid::fluid_storage_grid_options{
                .submap_coords = &submap_positions,
                .anchor = anchor_abs,
                .initial_state = initial_state,
                .buffer = &mb
            };
            auto storage_grid = make_shared_fast<fluid_storage_grid>( options );
            std::ranges::for_each( submap_positions, [&]( const tripoint_abs_sm & smp ) {
                parent_storage_grids[smp] = storage_grid;
            } );

            return *parent_storage_grids[submap_positions.front()];
        }

    public:
        fluid_grid_tracker() : fluid_grid_tracker( MAPBUFFER ) {}

        explicit fluid_grid_tracker( mapbuffer &buffer ) : mb( buffer ) {}

        auto load( const map &m ) -> void {
            const auto p_min = point_abs_sm( m.get_abs_sub().xy() );
            const auto p_max = p_min + point( m.getmapsize(), m.getmapsize() );
            bounds = half_open_rectangle<point_abs_sm>( p_min, p_max );
            rebuild_transformer_grids();
        }

        auto update( time_point to ) -> void {
            if( !bounds ) {
                return;
            }
            std::ranges::for_each( grids_requiring_updates, [&]( const tripoint_abs_omt & omt_pos ) {
                fluid_grid::process_transformers_at( omt_pos, to );
            } );
        }

        auto update_transformers_at( const tripoint_abs_ms &p ) -> void {
            update_transformer_grid_at( project_to<coords::omt>( p ) );
        }

        auto storage_at( const tripoint_abs_omt &p ) -> fluid_storage_grid& { // *NOPAD*
            const auto sm_pos = project_to<coords::sm>( p );
            const auto iter = parent_storage_grids.find( sm_pos );
            if( iter != parent_storage_grids.end() ) {
                return *iter->second;
            }

            return make_storage_grid_at( sm_pos );
        }

        auto invalidate_at( const tripoint_abs_ms &p ) -> void {
            const auto sm_pos = project_to<coords::sm>( p );
            const auto iter = parent_storage_grids.find( sm_pos );
            if( iter != parent_storage_grids.end() ) {
                iter->second->invalidate();
            }
        }

        auto rebuild_at( const tripoint_abs_ms &p ) -> void {
            const auto sm_pos = project_to<coords::sm>( p );
            invalidate_submap_cache_at( sm_pos );
            make_storage_grid_at( sm_pos );
        }

        auto disconnect_tank_at( const tripoint_abs_ms &p ) -> void {
            auto target_sm = tripoint_abs_sm{};
            auto target_pos = point_sm_ms{};
            std::tie( target_sm, target_pos ) = project_remain<coords::sm>( p );
            auto *target_submap = mb.lookup_submap( target_sm );
            if( target_submap == nullptr ) {
                return;
            }

            const auto &furn = target_submap->get_furn( target_pos.raw() ).obj();
            const auto tank_capacity = tank_capacity_for_furn( furn );
            if( !tank_capacity ) {
                return;
            }

            invalidate_submap_cache_at( target_sm );

            auto &grid = storage_at( project_to<coords::omt>( p ) );
            auto state = grid.get_state();
            const auto omt_pos = project_to<coords::omt>( p );
            const auto new_capacity = state.capacity > *tank_capacity
                                      ? state.capacity - *tank_capacity
                                      : 0_ml;
            const auto overflow_volume = state.stored_total() > new_capacity
                                         ? state.stored_total() - new_capacity
                                         : 0_ml;
            auto overflow = reduce_storage( state, overflow_volume );
            state.capacity = new_capacity;
            const auto grid_positions = fluid_grid::grid_at( omt_pos );
            const auto tank_count =
                tank_count_for_grid_excluding( grid_positions, mb, std::optional{ p } );
            enforce_tank_type_limit( state, tank_count );
            grid.set_state( state );

            auto &items = target_submap->get_items( target_pos.raw() );
            items.clear();

            std::ranges::for_each( overflow.stored_by_type, [&]( const auto & entry ) {
                if( entry.second <= 0_ml ) {
                    return;
                }
                auto liquid_item = item::spawn( entry.first, calendar::turn );
                liquid_item->charges = liquid_item->charges_per_volume( entry.second );
                items.push_back( std::move( liquid_item ) );
            } );
        }

        auto clear() -> void {
            parent_storage_grids.clear();
            grids_requiring_updates.clear();
            bounds.reset();
        }
};

auto get_fluid_grid_tracker() -> fluid_grid_tracker & // *NOPAD*
{
    static auto tracker = fluid_grid_tracker{};
    return tracker;
}

} // namespace

namespace fluid_grid
{

auto connections_for( overmap &om ) -> connection_map & // *NOPAD*
{
    return fluid_grid_store()[om.pos()];
}

auto connections_for( const overmap &om ) -> const connection_map & // *NOPAD*
{
    const auto &store = fluid_grid_store();
    const auto iter = store.find( om.pos() );
    if( iter == store.end() ) {
        return empty_connections();
    }
    return iter->second;
}

auto storage_for( overmap &om ) -> std::map<tripoint_om_omt, liquid_storage_state> & // *NOPAD*
{
    return fluid_storage_store()[om.pos()];
}

auto storage_for( const overmap &om ) -> const std::map<tripoint_om_omt, liquid_storage_state>
& // *NOPAD*
{
    const auto &store = fluid_storage_store();
    const auto iter = store.find( om.pos() );
    if( iter == store.end() ) {
        return empty_storage();
    }
    return iter->second;
}

auto grid_at( const tripoint_abs_omt &p ) -> std::set<tripoint_abs_omt>
{
    const auto omc = overmap_buffer.get_om_global( p );
    const auto &grid_members = grid_members_for( p );
    auto result = std::set<tripoint_abs_omt> {};
    std::ranges::for_each( grid_members, [&]( const tripoint_om_omt & member ) {
        result.emplace( project_combine( omc.om->pos(), member ) );
    } );

    return result;
}

auto grid_connectivity_at( const tripoint_abs_omt &p ) -> std::vector<tripoint_rel_omt>
{
    auto ret = std::vector<tripoint_rel_omt> {};
    ret.reserve( six_cardinal_directions.size() );

    const auto &connections_bitset = connection_bitset_at( p );
    std::ranges::for_each( std::views::iota( size_t{ 0 }, six_cardinal_directions.size() ),
    [&]( size_t i ) {
        if( connections_bitset.test( i ) ) {
            ret.emplace_back( six_cardinal_directions[i] );
        }
    } );

    return ret;
}

auto storage_stats_at( const tripoint_abs_omt &p ) -> liquid_storage_stats
{
    return get_fluid_grid_tracker().storage_at( p ).get_stats();
}

auto liquid_charges_at( const tripoint_abs_omt &p, const itype_id &liquid_type ) -> int
{
    return get_fluid_grid_tracker().storage_at( p ).total_charges( liquid_type );
}

auto would_contaminate( const tripoint_abs_omt &p, const itype_id &liquid_type ) -> bool
{
    const auto state = get_fluid_grid_tracker().storage_at( p ).get_state();
    const auto grid = grid_at( p );
    const auto allow_mixed = tank_count_for_grid( grid, MAPBUFFER ) > 1;
    return std::ranges::any_of( state.stored_by_type, [&]( const auto & entry ) {
        if( entry.second <= 0_ml ) {
            return false;
        }
        if( entry.first == liquid_type ) {
            return false;
        }
        if( allow_mixed ) {
            const auto is_water_pair =
                ( entry.first == itype_water && liquid_type == itype_water_clean ) ||
                ( entry.first == itype_water_clean && liquid_type == itype_water );
            if( is_water_pair ) {
                return false;
            }
        }
        return true;
    } );
}

auto add_liquid_charges( const tripoint_abs_omt &p, const itype_id &liquid_type,
                         int charges ) -> int
{
    return get_fluid_grid_tracker().storage_at( p ).add_charges( liquid_type, charges );
}

auto seed_liquid_charges_for_mapgen( const tripoint_abs_omt &p, const itype_id &liquid_type,
                                     int charges ) -> int
{
    if( charges <= 0 ) {
        return 0;
    }
    if( !is_supported_liquid( liquid_type ) ) {
        return 0;
    }

    const auto grid = grid_at( p );
    if( grid.empty() ) {
        return 0;
    }

    const auto anchor_abs = anchor_for_grid( grid );
    auto omc = overmap_buffer.get_om_global( anchor_abs );
    auto &storage = fluid_grid::storage_for( *omc.om );
    auto &state = storage[omc.local];
    state.capacity = calculate_capacity_for_grid( grid, MAPBUFFER );

    if( state.stored_total() > state.capacity ) {
        reduce_storage( state, state.stored_total() - state.capacity );
    }

    const auto allow_mixed = tank_count_for_grid( grid, MAPBUFFER ) > 1;
    normalize_water_storage( state, allow_mixed );
    if( liquid_type == itype_water ) {
        taint_clean_water( state, allow_mixed );
    }

    const auto available_volume = state.capacity - state.stored_total();
    if( available_volume <= 0_ml ) {
        return 0;
    }

    const auto max_charges = charges_from_volume( liquid_type, available_volume );
    const auto added = std::min( charges, max_charges );
    if( added <= 0 ) {
        return 0;
    }

    const auto added_volume = volume_from_charges( liquid_type, added );
    if( liquid_type == itype_water_clean ) {
        const auto dirty_iter = state.stored_by_type.find( itype_water );
        if( dirty_iter != state.stored_by_type.end() ) {
            dirty_iter->second += added_volume;
        } else {
            state.stored_by_type[itype_water_clean] += added_volume;
        }
    } else {
        state.stored_by_type[itype_water] += added_volume;
    }

    const auto tank_count = tank_count_for_grid( grid, MAPBUFFER );
    enforce_tank_type_limit( state, tank_count );
    return added;
}

auto drain_liquid_charges( const tripoint_abs_omt &p, const itype_id &liquid_type,
                           int charges ) -> int
{
    return get_fluid_grid_tracker().storage_at( p ).drain_charges( liquid_type, charges );
}

auto purify_water( const tripoint_abs_omt &p ) -> units::volume
{
    auto &grid = get_fluid_grid_tracker().storage_at( p );
    auto state = grid.get_state();
    const auto dirty_iter = state.stored_by_type.find( itype_water );
    if( dirty_iter == state.stored_by_type.end() ) {
        return 0_ml;
    }

    const auto dirty_volume = dirty_iter->second;
    state.stored_by_type.erase( dirty_iter );
    if( dirty_volume <= 0_ml ) {
        grid.set_state( state );
        return 0_ml;
    }

    state.stored_by_type[itype_water_clean] += dirty_volume;
    grid.set_state( state );
    return dirty_volume;
}

auto process_transformers_at( const tripoint_abs_omt &p, time_point to ) -> void
{
    const auto grid = grid_at( p );
    if( grid.empty() ) {
        return;
    }

    auto transformers = collect_transformers( grid, MAPBUFFER );
    if( transformers.empty() ) {
        return;
    }

    auto &storage = get_fluid_grid_tracker().storage_at( p );
    auto state = storage.get_state();
    const auto available = state.stored_by_type;

    struct transform_request {
        const fluid_grid_transform_recipe *recipe = nullptr;
        double batches = 0.0;
    };

    auto requests = std::vector<transform_request> {};
    auto processed_transformers = std::vector<tripoint_abs_ms> {};
    auto state_dirty = false;
    std::ranges::for_each( transformers, [&]( const transformer_instance & inst ) {
        const auto last_run = transformer_last_run_at( inst.pos );
        const auto tick_count = ticks_between( last_run, to, inst.config->tick_interval );
        if( tick_count <= 0 ) {
            return;
        }
        processed_transformers.push_back( inst.pos );
        if( inst.role == fluid_grid_role::rain_collector ) {
            if( !is_outdoors_at( inst.pos ) ) {
                return;
            }
            const auto weather = sum_conditions( last_run, to, inst.pos.raw() );
            if( weather.rain_amount <= 0 ) {
                return;
            }
            const auto charges = rain_charges_for( inst.config->collector_area_m2, weather );
            if( charges <= 0 ) {
                return;
            }
            const auto allow_mixed = tank_count_for_grid( grid, MAPBUFFER ) > 1;
            normalize_water_storage( state, allow_mixed );
            taint_clean_water( state, allow_mixed );
            const auto available_volume = state.capacity - state.stored_total();
            if( available_volume <= 0_ml ) {
                return;
            }
            const auto max_charges = charges_from_volume( itype_water, available_volume );
            const auto added_charges = std::min( charges, max_charges );
            if( added_charges <= 0 ) {
                return;
            }
            const auto added_volume = volume_from_charges( itype_water, added_charges );
            state.stored_by_type[itype_water] += added_volume;
            state_dirty = true;
            return;
        }

        std::ranges::for_each( inst.config->transforms,
        [&]( const fluid_grid_transform_recipe & recipe ) {
            auto capped_batches = recipe.inputs.empty()
                                  ? std::numeric_limits<double>::max()
                                  : batches_for_inputs( recipe.inputs, available );
            if( capped_batches <= 0.0 ) {
                return;
            }
            capped_batches = std::min( capped_batches, static_cast<double>( tick_count ) );
            requests.push_back( transform_request{ .recipe = &recipe, .batches = capped_batches } );
        } );
    } );

    if( requests.empty() ) {
        if( state_dirty ) {
            storage.set_state( state );
        }
        std::ranges::for_each( processed_transformers, [&]( const tripoint_abs_ms & pos ) {
            set_transformer_last_run_at( pos, to );
        } );
        return;
    }

    auto total_inputs_ml = std::map<itype_id, double> {};
    auto total_outputs_ml = std::map<itype_id, double> {};
    auto total_input_ml = 0.0;
    auto total_output_ml = 0.0;
    std::ranges::for_each( requests, [&]( const transform_request & request ) {
        std::ranges::for_each( request.recipe->inputs, [&]( const fluid_grid_transform_io & io ) {
            const auto volume_ml = units::to_milliliter<double>( io.amount ) * request.batches;
            total_inputs_ml[io.liquid] += volume_ml;
            total_input_ml += volume_ml;
        } );
        std::ranges::for_each( request.recipe->outputs, [&]( const fluid_grid_transform_io & io ) {
            const auto volume_ml = units::to_milliliter<double>( io.amount ) * request.batches;
            total_outputs_ml[io.liquid] += volume_ml;
            total_output_ml += volume_ml;
        } );
    } );

    auto scale = 1.0;
    std::ranges::for_each( total_inputs_ml, [&]( const auto & entry ) {
        const auto available_iter = available.find( entry.first );
        const auto available_volume = available_iter == available.end()
                                      ? 0_ml
                                      : available_iter->second;
        if( entry.second <= units::to_milliliter<double>( available_volume ) ) {
            return;
        }
        const auto avail_ml = units::to_milliliter<double>( available_volume );
        if( entry.second > 0.0 ) {
            scale = std::min( scale, avail_ml / entry.second );
        }
    } );

    if( total_output_ml > 0.0 ) {
        const auto available_capacity = state.capacity - state.stored_total();
        const auto max_output_ml = total_input_ml > 0.0
                                   ? units::to_milliliter<double>( available_capacity ) + total_input_ml
                                   : units::to_milliliter<double>( available_capacity );
        if( total_output_ml > max_output_ml && max_output_ml > 0.0 ) {
            scale = std::min( scale, max_output_ml / total_output_ml );
        }
    }

    if( scale <= 0.0 ) {
        return;
    }

    std::ranges::for_each( requests, [&]( const transform_request & request ) {
        const auto scaled_batches = request.batches * scale;
        if( scaled_batches <= 0.0 ) {
            return;
        }
        auto adjusted_batches = std::numeric_limits<double>::max();
        if( request.recipe->inputs.empty() ) {
            adjusted_batches = scaled_batches;
        }
        std::ranges::for_each( request.recipe->inputs, [&]( const fluid_grid_transform_io & io ) {
            const auto amount_ml = units::to_milliliter<double>( io.amount );
            if( amount_ml <= 0.0 ) {
                adjusted_batches = 0.0;
                return;
            }
            const auto volume_ml = amount_ml * scaled_batches;
            if( volume_ml <= 0.0 ) {
                adjusted_batches = 0.0;
                return;
            }
            adjusted_batches = std::min( adjusted_batches,
                                         std::floor( volume_ml ) / amount_ml );
        } );

        if( adjusted_batches <= 0.0 || adjusted_batches == std::numeric_limits<double>::max() ) {
            return;
        }

        std::ranges::for_each( request.recipe->inputs, [&]( const fluid_grid_transform_io & io ) {
            const auto amount_ml = units::to_milliliter<double>( io.amount );
            const auto volume_ml = amount_ml * adjusted_batches;
            const auto volume_ml_floor = static_cast<int>( std::floor( volume_ml ) );
            if( volume_ml_floor <= 0 ) {
                return;
            }
            const auto volume = units::from_milliliter( volume_ml_floor );
            if( volume <= 0_ml ) {
                return;
            }
            const auto iter = state.stored_by_type.find( io.liquid );
            if( iter == state.stored_by_type.end() ) {
                return;
            }
            iter->second -= std::min( iter->second, volume );
            if( iter->second <= 0_ml ) {
                state.stored_by_type.erase( iter );
            }
        } );
        std::ranges::for_each( request.recipe->outputs, [&]( const fluid_grid_transform_io & io ) {
            const auto amount_ml = units::to_milliliter<double>( io.amount );
            const auto volume_ml = amount_ml * adjusted_batches;
            const auto volume_ml_floor = static_cast<int>( std::floor( volume_ml ) );
            if( volume_ml_floor <= 0 ) {
                return;
            }
            const auto volume = units::from_milliliter( volume_ml_floor );
            if( volume <= 0_ml ) {
                return;
            }
            state.stored_by_type[io.liquid] += volume;
        } );
    } );

    storage.set_state( state );

    std::ranges::for_each( processed_transformers, [&]( const tripoint_abs_ms & pos ) {
        set_transformer_last_run_at( pos, to );
    } );
}

auto update( time_point to ) -> void
{
    get_fluid_grid_tracker().update( to );
}

auto load( const map &m ) -> void
{
    get_fluid_grid_tracker().load( m );
}

auto on_contents_changed( const tripoint_abs_ms &p ) -> void
{
    get_fluid_grid_tracker().invalidate_at( p );
}

auto on_structure_changed( const tripoint_abs_ms &p ) -> void
{
    const auto sm_pos = project_to<coords::sm>( p );
    // Mapgen mutates temporary submaps before they are committed to MAPBUFFER.
    // Ignore those transient updates so we don't cache zero-capacity grids.
    if( MAPBUFFER.lookup_submap( sm_pos ) == nullptr ) {
        return;
    }

    const auto omt_pos = project_to<coords::omt>( p );
    invalidate_transformer_cache_at( sm_pos );
    invalidate_grid_members_cache_at( omt_pos );
    get_fluid_grid_tracker().rebuild_at( p );
    get_fluid_grid_tracker().update_transformers_at( p );
    if( has_transformer_at( MAPBUFFER, p ) ) {
        set_transformer_last_run_at( p, calendar::turn );
    } else {
        clear_transformer_last_run_at( p );
    }
}

auto disconnect_tank( const tripoint_abs_ms &p ) -> void
{
    get_fluid_grid_tracker().disconnect_tank_at( p );
}

auto on_tank_removed( const tripoint_abs_ms &p ) -> void
{
    const auto tank_capacity = tank_capacity_at( MAPBUFFER, p );
    if( !tank_capacity ) {
        return;
    }

    invalidate_submap_cache_at( project_to<coords::sm>( p ) );

    const auto omt_pos = project_to<coords::omt>( p );
    auto &grid = get_fluid_grid_tracker().storage_at( omt_pos );
    auto state = grid.get_state();
    const auto new_capacity = state.capacity > *tank_capacity
                              ? state.capacity - *tank_capacity
                              : 0_ml;
    const auto overflow_volume = state.stored_total() > new_capacity
                                 ? state.stored_total() - new_capacity
                                 : 0_ml;
    auto overflow = reduce_storage( state, overflow_volume );
    state.capacity = new_capacity;
    const auto grid_positions = fluid_grid::grid_at( omt_pos );
    const auto tank_count = tank_count_for_grid( grid_positions, MAPBUFFER );
    enforce_tank_type_limit( state, tank_count );
    grid.set_state( state );

    auto target_sm = tripoint_abs_sm{};
    auto target_pos = point_sm_ms{};
    std::tie( target_sm, target_pos ) = project_remain<coords::sm>( p );
    auto *target_submap = MAPBUFFER.lookup_submap( target_sm );
    if( target_submap == nullptr ) {
        return;
    }

    auto &items = target_submap->get_items( target_pos.raw() );
    std::ranges::for_each( overflow.stored_by_type, [&]( const auto & entry ) {
        if( entry.second <= 0_ml ) {
            return;
        }
        auto liquid_item = item::spawn( entry.first, calendar::turn );
        liquid_item->charges = liquid_item->charges_per_volume( entry.second );
        items.push_back( std::move( liquid_item ) );
    } );
}

auto add_grid_connection( const tripoint_abs_omt &lhs, const tripoint_abs_omt &rhs ) -> bool
{
    if( project_to<coords::om>( lhs ).xy() != project_to<coords::om>( rhs ).xy() ) {
        debugmsg( "Connecting fluid grids on different overmaps is not supported yet" );
        return false;
    }

    const auto coord_diff = rhs - lhs;
    if( std::abs( coord_diff.x() ) + std::abs( coord_diff.y() ) + std::abs( coord_diff.z() ) != 1 ) {
        debugmsg( "Tried to connect non-orthogonally adjacent points" );
        return false;
    }

    auto lhs_omc = overmap_buffer.get_om_global( lhs );
    auto rhs_omc = overmap_buffer.get_om_global( rhs );

    const auto lhs_iter = std::ranges::find( six_cardinal_directions, coord_diff.raw() );
    const auto rhs_iter = std::ranges::find( six_cardinal_directions, -coord_diff.raw() );

    auto lhs_i = static_cast<size_t>( std::distance( six_cardinal_directions.begin(), lhs_iter ) );
    auto rhs_i = static_cast<size_t>( std::distance( six_cardinal_directions.begin(), rhs_iter ) );

    auto &lhs_bitset = connection_bitset_at( *lhs_omc.om, lhs_omc.local );
    auto &rhs_bitset = connection_bitset_at( *rhs_omc.om, rhs_omc.local );

    if( lhs_bitset[lhs_i] && rhs_bitset[rhs_i] ) {
        debugmsg( "Tried to connect to fluid grid two points that are connected to each other" );
        return false;
    }

    const auto lhs_grid = grid_at( lhs );
    const auto rhs_grid = grid_at( rhs );
    const auto same_grid = lhs_grid.contains( rhs );
    const auto lhs_state = storage_state_for_grid( lhs_grid );
    const auto rhs_state = storage_state_for_grid( rhs_grid );

    lhs_bitset[lhs_i] = true;
    rhs_bitset[rhs_i] = true;

    if( !same_grid ) {
        invalidate_grid_members_cache_at( lhs );
        const auto merged_grid = grid_at( lhs );
        const auto new_anchor = anchor_for_grid( merged_grid );
        auto new_omc = overmap_buffer.get_om_global( new_anchor );
        auto &storage = fluid_grid::storage_for( *new_omc.om );
        storage.erase( overmap_buffer.get_om_global( anchor_for_grid( lhs_grid ) ).local );
        storage.erase( overmap_buffer.get_om_global( anchor_for_grid( rhs_grid ) ).local );
        storage[new_omc.local] = merge_storage_states( lhs_state, rhs_state );
    }

    on_structure_changed( project_to<coords::ms>( lhs ) );
    on_structure_changed( project_to<coords::ms>( rhs ) );
    return true;
}

auto remove_grid_connection( const tripoint_abs_omt &lhs, const tripoint_abs_omt &rhs ) -> bool
{
    const auto coord_diff = rhs - lhs;
    if( std::abs( coord_diff.x() ) + std::abs( coord_diff.y() ) + std::abs( coord_diff.z() ) != 1 ) {
        debugmsg( "Tried to disconnect non-orthogonally adjacent points" );
        return false;
    }

    auto lhs_omc = overmap_buffer.get_om_global( lhs );
    auto rhs_omc = overmap_buffer.get_om_global( rhs );

    const auto lhs_iter = std::ranges::find( six_cardinal_directions, coord_diff.raw() );
    const auto rhs_iter = std::ranges::find( six_cardinal_directions, -coord_diff.raw() );

    auto lhs_i = static_cast<size_t>( std::distance( six_cardinal_directions.begin(), lhs_iter ) );
    auto rhs_i = static_cast<size_t>( std::distance( six_cardinal_directions.begin(), rhs_iter ) );

    auto &lhs_bitset = connection_bitset_at( *lhs_omc.om, lhs_omc.local );
    auto &rhs_bitset = connection_bitset_at( *rhs_omc.om, rhs_omc.local );

    if( !lhs_bitset[lhs_i] && !rhs_bitset[rhs_i] ) {
        debugmsg( "Tried to disconnect from fluid grid two points with no connection to each other" );
        return false;
    }

    const auto old_grid = grid_at( lhs );
    const auto old_state = storage_state_for_grid( old_grid );
    const auto old_anchor = anchor_for_grid( old_grid );

    lhs_bitset[lhs_i] = false;
    rhs_bitset[rhs_i] = false;

    invalidate_grid_members_cache_at( lhs );
    const auto lhs_grid = grid_at( lhs );
    if( !lhs_grid.contains( rhs ) ) {
        const auto rhs_grid = grid_at( rhs );
        const auto lhs_capacity = calculate_capacity_for_grid( lhs_grid, MAPBUFFER );
        const auto rhs_capacity = calculate_capacity_for_grid( rhs_grid, MAPBUFFER );
        const auto split_state = split_storage_state( old_state, lhs_capacity, rhs_capacity );

        auto &storage = fluid_grid::storage_for( *lhs_omc.om );
        storage.erase( overmap_buffer.get_om_global( old_anchor ).local );
        const auto lhs_anchor = anchor_for_grid( lhs_grid );
        const auto rhs_anchor = anchor_for_grid( rhs_grid );
        storage[overmap_buffer.get_om_global( lhs_anchor ).local] = split_state.lhs;
        storage[overmap_buffer.get_om_global( rhs_anchor ).local] = split_state.rhs;
    }

    on_structure_changed( project_to<coords::ms>( lhs ) );
    on_structure_changed( project_to<coords::ms>( rhs ) );
    return true;
}

auto clear() -> void
{
    fluid_grid_store().clear();
    fluid_storage_store().clear();
    get_fluid_grid_tracker().clear();
    grid_members_cache().clear();
    submap_tank_cache().clear();
    transformer_cache().clear();
}

} // namespace fluid_grid
