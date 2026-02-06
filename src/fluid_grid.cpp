#include "fluid_grid.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <memory>
#include <optional>
#include <queue>
#include <ranges>

#include "calendar.h"
#include "coordinate_conversions.h"
#include "coordinates.h"
#include "debug.h"
#include "game_constants.h"
#include "item.h"
#include "mapbuffer.h"
#include "mapdata.h"
#include "memory_fast.h"
#include "overmap.h"
#include "overmapbuffer.h"
#include "point.h"
#include "submap.h"

namespace
{

using connection_store = std::map<point_abs_om, fluid_grid::connection_map>;
using storage_store =
    std::map<point_abs_om, std::map<tripoint_om_omt, fluid_grid::liquid_storage_state>>;
using grid_member_set = std::set<tripoint_om_omt>;
using grid_member_ptr = shared_ptr_fast<grid_member_set>;
using grid_member_map = std::map<tripoint_om_omt, grid_member_ptr>;
using grid_member_cache_store = std::map<point_abs_om, grid_member_map>;
using submap_capacity_cache_store = std::map<tripoint_abs_sm, units::volume>;
static const itype_id itype_water( "water" );
static const itype_id itype_water_clean( "water_clean" );

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

auto submap_capacity_cache() -> submap_capacity_cache_store & // *NOPAD*
{
    static auto cache = submap_capacity_cache_store{};
    return cache;
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

auto normalize_water_storage( fluid_grid::liquid_storage_state &state ) -> void
{
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

auto taint_clean_water( fluid_grid::liquid_storage_state &state ) -> void
{
    auto clean_iter = state.stored_by_type.find( itype_water_clean );
    if( clean_iter == state.stored_by_type.end() ) {
        return;
    }
    state.stored_by_type[itype_water] += clean_iter->second;
    state.stored_by_type.erase( clean_iter );
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

auto invalidate_submap_capacity_cache_at( const tripoint_abs_sm &p ) -> void
{
    submap_capacity_cache().erase( p );
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

auto submap_capacity_at( const tripoint_abs_sm &sm_coord, mapbuffer &mb ) -> units::volume
{
    auto &cache = submap_capacity_cache();
    const auto iter = cache.find( sm_coord );
    if( iter != cache.end() ) {
        return iter->second;
    }

    auto *sm = mb.lookup_submap( sm_coord );
    if( sm == nullptr ) {
        return 0_ml;
    }

    auto total = 0_ml;
    std::ranges::for_each( std::views::iota( 0, SEEX ), [&]( int x ) {
        std::ranges::for_each( std::views::iota( 0, SEEY ), [&]( int y ) {
            const auto pos = point( x, y );
            const auto &furn = sm->get_furn( pos ).obj();
            const auto tank_capacity = tank_capacity_for_furn( furn );
            if( tank_capacity ) {
                total += *tank_capacity;
            }
        } );
    } );

    cache.emplace( sm_coord, total );
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
        total += submap_capacity_at( sm_coord, mb );
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

            normalize_water_storage( state );
            if( liquid_type == itype_water ) {
                taint_clean_water( state );
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
            normalize_water_storage( state );
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
                total += submap_capacity_at( sm_coord, mb );
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
            invalidate_submap_capacity_cache_at( sm_pos );
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

            auto &grid = storage_at( project_to<coords::omt>( p ) );
            auto state = grid.get_state();
            const auto new_capacity = state.capacity > *tank_capacity
                                      ? state.capacity - *tank_capacity
                                      : 0_ml;
            const auto overflow_volume = state.stored_total() > new_capacity
                                         ? state.stored_total() - new_capacity
                                         : 0_ml;
            auto overflow = reduce_storage( state, overflow_volume );
            state.capacity = new_capacity;
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
    return std::ranges::any_of( state.stored_by_type, [&]( const auto & entry ) {
        return entry.second > 0_ml && entry.first != liquid_type;
    } );
}

auto add_liquid_charges( const tripoint_abs_omt &p, const itype_id &liquid_type,
                         int charges ) -> int
{
    return get_fluid_grid_tracker().storage_at( p ).add_charges( liquid_type, charges );
}

auto drain_liquid_charges( const tripoint_abs_omt &p, const itype_id &liquid_type,
                           int charges ) -> int
{
    return get_fluid_grid_tracker().storage_at( p ).drain_charges( liquid_type, charges );
}

auto on_contents_changed( const tripoint_abs_ms &p ) -> void
{
    get_fluid_grid_tracker().invalidate_at( p );
}

auto on_structure_changed( const tripoint_abs_ms &p ) -> void
{
    const auto omt_pos = project_to<coords::omt>( p );
    invalidate_grid_members_cache_at( omt_pos );
    get_fluid_grid_tracker().rebuild_at( p );
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

    auto &grid = get_fluid_grid_tracker().storage_at( project_to<coords::omt>( p ) );
    auto state = grid.get_state();
    const auto new_capacity = state.capacity > *tank_capacity
                              ? state.capacity - *tank_capacity
                              : 0_ml;
    const auto overflow_volume = state.stored_total() > new_capacity
                                 ? state.stored_total() - new_capacity
                                 : 0_ml;
    auto overflow = reduce_storage( state, overflow_volume );
    state.capacity = new_capacity;
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
    submap_capacity_cache().clear();
}

} // namespace fluid_grid
