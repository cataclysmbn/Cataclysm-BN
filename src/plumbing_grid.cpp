#include "plumbing_grid.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
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

using connection_store = std::map<point_abs_om, plumbing_grid::connection_map>;
using storage_store = std::map<point_abs_om, std::map<tripoint_om_omt, plumbing_grid::water_storage_state>>;
static const auto furn_standing_tank_plumbed_str = furn_str_id( "f_standing_tank_plumbed" );
static const itype_id itype_water( "water" );
static const itype_id itype_water_clean( "water_clean" );

auto plumbing_grid_store() -> connection_store &
{
    static auto store = connection_store{};
    return store;
}

auto empty_connections() -> const plumbing_grid::connection_map &
{
    static const auto empty = plumbing_grid::connection_map{};
    return empty;
}

auto plumbing_storage_store() -> storage_store &
{
    static auto store = storage_store{};
    return store;
}

auto empty_storage() -> const std::map<tripoint_om_omt, plumbing_grid::water_storage_state> &
{
    static const auto empty = std::map<tripoint_om_omt, plumbing_grid::water_storage_state>{};
    return empty;
}

auto empty_bitset() -> const plumbing_grid::connection_bitset &
{
    static const auto empty = plumbing_grid::connection_bitset{};
    return empty;
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

auto reduce_storage( plumbing_grid::water_storage_state &state,
                     units::volume volume ) -> plumbing_grid::water_storage_state
{
    auto remaining = volume;
    auto removed = plumbing_grid::water_storage_state{};

    if( remaining <= 0_ml ) {
        return removed;
    }

    const auto dirty_removed = std::min( state.stored_dirty, remaining );
    state.stored_dirty -= dirty_removed;
    removed.stored_dirty = dirty_removed;
    remaining -= dirty_removed;

    if( remaining > 0_ml ) {
        const auto clean_removed = std::min( state.stored_clean, remaining );
        state.stored_clean -= clean_removed;
        removed.stored_clean = clean_removed;
        remaining -= clean_removed;
    }

    return removed;
}

auto anchor_for_grid( const std::set<tripoint_abs_omt> &grid ) -> tripoint_abs_omt
{
    if( grid.empty() ) {
        return tripoint_abs_omt{ tripoint_zero };
    }

    return *std::ranges::min_element( grid, []( const tripoint_abs_omt &lhs,
    const tripoint_abs_omt &rhs ) {
        return lhs.raw() < rhs.raw();
    } );
}

auto collect_storage_for_grid( const tripoint_abs_omt &anchor_abs,
                               const std::set<tripoint_abs_omt> &grid ) -> plumbing_grid::water_storage_state
{
    auto total = plumbing_grid::water_storage_state{};
    if( grid.empty() ) {
        return total;
    }

    auto omc = overmap_buffer.get_om_global( anchor_abs );
    auto &storage = plumbing_grid::storage_for( *omc.om );
    auto to_erase = std::vector<tripoint_om_omt>{};

    std::ranges::for_each( storage, [&]( const auto &entry ) {
        const auto abs_pos = project_combine( omc.om->pos(), entry.first );
        if( grid.contains( abs_pos ) ) {
            total.stored_clean += entry.second.stored_clean;
            total.stored_dirty += entry.second.stored_dirty;
            to_erase.push_back( entry.first );
        }
    } );

    std::ranges::for_each( to_erase, [&]( const tripoint_om_omt &entry ) {
        storage.erase( entry );
    } );

    return total;
}

auto storage_state_for_grid( const std::set<tripoint_abs_omt> &grid ) -> plumbing_grid::water_storage_state
{
    if( grid.empty() ) {
        return {};
    }
    const auto anchor_abs = anchor_for_grid( grid );
    const auto omc = overmap_buffer.get_om_global( anchor_abs );
    const auto &storage = plumbing_grid::storage_for( *omc.om );
    const auto iter = storage.find( omc.local );
    if( iter == storage.end() ) {
        return {};
    }
    return iter->second;
}

auto calculate_capacity_for_grid( const std::set<tripoint_abs_omt> &grid,
                                  mapbuffer &mb ) -> units::volume
{
    auto submap_positions = std::set<tripoint_abs_sm>{};
    std::ranges::for_each( grid, [&]( const tripoint_abs_omt &omp ) {
        const auto base = project_to<coords::sm>( omp );
        submap_positions.emplace( base + point_zero );
        submap_positions.emplace( base + point_east );
        submap_positions.emplace( base + point_south );
        submap_positions.emplace( base + point_south_east );
    } );

    auto total = 0_ml;
    std::ranges::for_each( submap_positions, [&]( const tripoint_abs_sm &sm_coord ) {
        auto *sm = mb.lookup_submap( sm_coord );
        if( sm == nullptr ) {
            return;
        }

        std::ranges::for_each( std::views::iota( 0, SEEX ), [&]( int x ) {
            std::ranges::for_each( std::views::iota( 0, SEEY ), [&]( int y ) {
                const auto pos = point( x, y );
                if( sm->get_furn( pos ).id() == furn_standing_tank_plumbed_str ) {
                    total += sm->get_furn( pos ).obj().keg_capacity;
                }
            } );
        } );
    } );

    return total;
}

struct split_storage_result {
    plumbing_grid::water_storage_state lhs;
    plumbing_grid::water_storage_state rhs;
};

auto split_storage_state( const plumbing_grid::water_storage_state &state,
                          units::volume lhs_capacity,
                          units::volume rhs_capacity ) -> split_storage_result
{
    auto lhs_state = plumbing_grid::water_storage_state{ .capacity = lhs_capacity };
    auto rhs_state = plumbing_grid::water_storage_state{ .capacity = rhs_capacity };

    const auto total_capacity = lhs_capacity + rhs_capacity;
    if( total_capacity <= 0_ml ) {
        return { .lhs = lhs_state, .rhs = rhs_state };
    }

    const auto total_capacity_ml = units::to_milliliter<int>( total_capacity );
    const auto lhs_capacity_ml = units::to_milliliter<int>( lhs_capacity );
    const auto ratio = static_cast<double>( lhs_capacity_ml ) / total_capacity_ml;

    const auto clean_ml = units::to_milliliter<int>( state.stored_clean );
    const auto dirty_ml = units::to_milliliter<int>( state.stored_dirty );

    const auto lhs_clean_ml = static_cast<int>( std::round( clean_ml * ratio ) );
    const auto lhs_dirty_ml = static_cast<int>( std::round( dirty_ml * ratio ) );

    lhs_state.stored_clean = units::from_milliliter( lhs_clean_ml );
    lhs_state.stored_dirty = units::from_milliliter( lhs_dirty_ml );

    rhs_state.stored_clean = state.stored_clean - lhs_state.stored_clean;
    rhs_state.stored_dirty = state.stored_dirty - lhs_state.stored_dirty;

    return { .lhs = lhs_state, .rhs = rhs_state };
}

auto connection_bitset_at( const tripoint_abs_omt &p ) -> const plumbing_grid::connection_bitset &
{
    const auto omc = overmap_buffer.get_om_global( p );
    const auto &connections = plumbing_grid::connections_for( *omc.om );
    const auto iter = connections.find( omc.local );
    if( iter == connections.end() ) {
        return empty_bitset();
    }
    return iter->second;
}

auto connection_bitset_at( overmap &om, const tripoint_om_omt &p ) -> plumbing_grid::connection_bitset &
{
    auto &connections = plumbing_grid::connections_for( om );
    return connections[p];
}

class plumbing_storage_grid
{
    private:
        std::vector<tripoint_abs_sm> submap_coords;
        tripoint_abs_omt anchor_abs;
        plumbing_grid::water_storage_state state;
        mutable std::optional<plumbing_grid::water_storage_stats> cached_stats;

        mapbuffer &mb;

    public:
        struct plumbing_storage_grid_options {
            const std::vector<tripoint_abs_sm> *submap_coords = nullptr;
            tripoint_abs_omt anchor = tripoint_abs_omt{ tripoint_zero };
            plumbing_grid::water_storage_state initial_state;
            mapbuffer *buffer = nullptr;
        };

        explicit plumbing_storage_grid( const plumbing_storage_grid_options &opts ) :
            submap_coords( *opts.submap_coords ),
            anchor_abs( opts.anchor ),
            state( opts.initial_state ),
            mb( *opts.buffer )
        {
            state.capacity = calculate_capacity();
            if( state.stored_total() > state.capacity ) {
                reduce_storage( state, state.stored_total() - state.capacity );
            }
            sync_storage();
        }

        auto empty() const -> bool
        {
            return state.capacity <= 0_ml;
        }

        auto invalidate() -> void
        {
            cached_stats.reset();
        }

        auto get_stats() const -> plumbing_grid::water_storage_stats
        {
            if( cached_stats ) {
                return *cached_stats;
            }

            auto stats = plumbing_grid::water_storage_stats{
                .stored = std::min( state.stored_total(), state.capacity ),
                .capacity = state.capacity
            };

            cached_stats = stats;
            return stats;
        }

        auto total_charges( const itype_id &liquid_type ) const -> int
        {
            if( liquid_type == itype_water_clean ) {
                return charges_from_volume( liquid_type, state.stored_clean );
            }
            if( liquid_type == itype_water ) {
                return charges_from_volume( liquid_type, state.stored_dirty );
            }
            return 0;
        }

        auto drain_charges( const itype_id &liquid_type, int charges ) -> int
        {
            if( charges <= 0 ) {
                return 0;
            }
            const auto available = total_charges( liquid_type );
            const auto used = std::min( charges, available );
            if( used <= 0 ) {
                return 0;
            }

            const auto used_volume = volume_from_charges( liquid_type, used );
            if( liquid_type == itype_water_clean ) {
                state.stored_clean -= std::min( state.stored_clean, used_volume );
            } else if( liquid_type == itype_water ) {
                state.stored_dirty -= std::min( state.stored_dirty, used_volume );
            }

            cached_stats.reset();
            sync_storage();
            return used;
        }

        auto add_charges( const itype_id &liquid_type, int charges ) -> int
        {
            if( charges <= 0 ) {
                return 0;
            }
            if( liquid_type != itype_water && liquid_type != itype_water_clean ) {
                return 0;
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
                state.stored_clean += added_volume;
            } else {
                state.stored_dirty += added_volume;
            }
            cached_stats.reset();
            sync_storage();
            return added;
        }

        auto get_state() const -> plumbing_grid::water_storage_state
        {
            return state;
        }

        auto set_state( const plumbing_grid::water_storage_state &new_state ) -> void
        {
            state = new_state;
            cached_stats.reset();
            sync_storage();
        }

        auto set_capacity( units::volume capacity ) -> void
        {
            state.capacity = std::max( capacity, 0_ml );
            if( state.stored_total() > state.capacity ) {
                reduce_storage( state, state.stored_total() - state.capacity );
            }
            cached_stats.reset();
            sync_storage();
        }

    private:
        auto calculate_capacity() const -> units::volume
        {
            auto total = 0_ml;
            std::ranges::for_each( submap_coords, [&]( const tripoint_abs_sm &sm_coord ) {
                auto *sm = mb.lookup_submap( sm_coord );
                if( sm == nullptr ) {
                    return;
                }

                std::ranges::for_each( std::views::iota( 0, SEEX ), [&]( int x ) {
                    std::ranges::for_each( std::views::iota( 0, SEEY ), [&]( int y ) {
                        const auto pos = point( x, y );
                        if( sm->get_furn( pos ).id() == furn_standing_tank_plumbed_str ) {
                            total += sm->get_furn( pos ).obj().keg_capacity;
                        }
                    } );
                } );
            } );
            return total;
        }

        auto sync_storage() -> void
        {
            auto omc = overmap_buffer.get_om_global( anchor_abs );
            auto &storage = plumbing_grid::storage_for( *omc.om );
            storage[omc.local] = state;
        }
};

class plumbing_grid_tracker
{
    private:
        std::map<tripoint_abs_sm, shared_ptr_fast<plumbing_storage_grid>> parent_storage_grids;
        mapbuffer &mb;

        auto make_storage_grid_at( const tripoint_abs_sm &sm_pos ) -> plumbing_storage_grid &
        {
            const auto overmap_positions = plumbing_grid::grid_at( project_to<coords::omt>( sm_pos ) );
            auto submap_positions = std::vector<tripoint_abs_sm>{};
            submap_positions.reserve( overmap_positions.size() * 4 );

            std::ranges::for_each( overmap_positions, [&]( const tripoint_abs_omt &omp ) {
                const auto base = project_to<coords::sm>( omp );
                submap_positions.emplace_back( base + point_zero );
                submap_positions.emplace_back( base + point_east );
                submap_positions.emplace_back( base + point_south );
                submap_positions.emplace_back( base + point_south_east );
            } );

            if( submap_positions.empty() ) {
                static const auto empty_submaps = std::vector<tripoint_abs_sm>{};
                static const auto empty_options = plumbing_storage_grid::plumbing_storage_grid_options{
                    .submap_coords = &empty_submaps,
                    .anchor = tripoint_abs_omt{ tripoint_zero },
                    .initial_state = plumbing_grid::water_storage_state{},
                    .buffer = &MAPBUFFER
                };
                static auto empty_storage_grid = plumbing_storage_grid( empty_options );
                return empty_storage_grid;
            }

            const auto anchor_abs = anchor_for_grid( overmap_positions );
            const auto initial_state = collect_storage_for_grid( anchor_abs, overmap_positions );
            auto options = plumbing_storage_grid::plumbing_storage_grid_options{
                .submap_coords = &submap_positions,
                .anchor = anchor_abs,
                .initial_state = initial_state,
                .buffer = &mb
            };
            auto storage_grid = make_shared_fast<plumbing_storage_grid>( options );
            std::ranges::for_each( submap_positions, [&]( const tripoint_abs_sm &smp ) {
                parent_storage_grids[smp] = storage_grid;
            } );

            return *parent_storage_grids[submap_positions.front()];
        }

    public:
        plumbing_grid_tracker() : plumbing_grid_tracker( MAPBUFFER ) {}

        explicit plumbing_grid_tracker( mapbuffer &buffer ) : mb( buffer ) {}

        auto storage_at( const tripoint_abs_omt &p ) -> plumbing_storage_grid &
        {
            const auto sm_pos = project_to<coords::sm>( p );
            const auto iter = parent_storage_grids.find( sm_pos );
            if( iter != parent_storage_grids.end() ) {
                return *iter->second;
            }

            return make_storage_grid_at( sm_pos );
        }

        auto invalidate_at( const tripoint_abs_ms &p ) -> void
        {
            const auto sm_pos = project_to<coords::sm>( p );
            const auto iter = parent_storage_grids.find( sm_pos );
            if( iter != parent_storage_grids.end() ) {
                iter->second->invalidate();
            }
        }

        auto rebuild_at( const tripoint_abs_ms &p ) -> void
        {
            const auto sm_pos = project_to<coords::sm>( p );
            make_storage_grid_at( sm_pos );
        }

        auto disconnect_tank_at( const tripoint_abs_ms &p ) -> void
        {
            auto target_sm = tripoint_abs_sm{};
            auto target_pos = point_sm_ms{};
            std::tie( target_sm, target_pos ) = project_remain<coords::sm>( p );
            auto *target_submap = mb.lookup_submap( target_sm );
            if( target_submap == nullptr ) {
                return;
            }

            const auto &furn = target_submap->get_furn( target_pos.raw() ).obj();
            const auto tank_capacity = furn.keg_capacity;

            auto &grid = storage_at( project_to<coords::omt>( p ) );
            auto state = grid.get_state();
            const auto new_capacity = state.capacity > tank_capacity
                                      ? state.capacity - tank_capacity
                                      : 0_ml;
            const auto overflow_volume = state.stored_total() > new_capacity
                                         ? state.stored_total() - new_capacity
                                         : 0_ml;
            auto overflow = reduce_storage( state, overflow_volume );
            state.capacity = new_capacity;
            grid.set_state( state );

            auto &items = target_submap->get_items( target_pos.raw() );
            items.clear();

            if( overflow.stored_dirty > 0_ml ) {
                auto liquid_item = item::spawn( itype_water, calendar::turn );
                liquid_item->charges = liquid_item->charges_per_volume( overflow.stored_dirty );
                items.push_back( std::move( liquid_item ) );
            }
            if( overflow.stored_clean > 0_ml ) {
                auto liquid_item = item::spawn( itype_water_clean, calendar::turn );
                liquid_item->charges = liquid_item->charges_per_volume( overflow.stored_clean );
                items.push_back( std::move( liquid_item ) );
            }
        }

        auto clear() -> void
        {
            parent_storage_grids.clear();
        }
};

auto get_plumbing_grid_tracker() -> plumbing_grid_tracker &
{
    static auto tracker = plumbing_grid_tracker{};
    return tracker;
}

} // namespace

namespace plumbing_grid
{

auto connections_for( overmap &om ) -> connection_map &
{
    return plumbing_grid_store()[om.pos()];
}

auto connections_for( const overmap &om ) -> const connection_map &
{
    const auto &store = plumbing_grid_store();
    const auto iter = store.find( om.pos() );
    if( iter == store.end() ) {
        return empty_connections();
    }
    return iter->second;
}

auto storage_for( overmap &om ) -> std::map<tripoint_om_omt, water_storage_state> &
{
    return plumbing_storage_store()[om.pos()];
}

auto storage_for( const overmap &om ) -> const std::map<tripoint_om_omt, water_storage_state> &
{
    const auto &store = plumbing_storage_store();
    const auto iter = store.find( om.pos() );
    if( iter == store.end() ) {
        return empty_storage();
    }
    return iter->second;
}

auto grid_at( const tripoint_abs_omt &p ) -> std::set<tripoint_abs_omt>
{
    auto result = std::set<tripoint_abs_omt>{};
    auto open = std::queue<tripoint_abs_omt>{};
    open.emplace( p );

    while( !open.empty() ) {
        const auto &elem = open.front();
        result.emplace( elem );
        const auto &connections_bitset = connection_bitset_at( elem );
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

auto grid_connectivity_at( const tripoint_abs_omt &p ) -> std::vector<tripoint_rel_omt>
{
    auto ret = std::vector<tripoint_rel_omt>{};
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

auto water_storage_at( const tripoint_abs_omt &p ) -> water_storage_stats
{
    return get_plumbing_grid_tracker().storage_at( p ).get_stats();
}

auto liquid_charges_at( const tripoint_abs_omt &p, const itype_id &liquid_type ) -> int
{
    return get_plumbing_grid_tracker().storage_at( p ).total_charges( liquid_type );
}

auto add_liquid_charges( const tripoint_abs_omt &p, const itype_id &liquid_type,
                         int charges ) -> int
{
    return get_plumbing_grid_tracker().storage_at( p ).add_charges( liquid_type, charges );
}

auto drain_liquid_charges( const tripoint_abs_omt &p, const itype_id &liquid_type,
                           int charges ) -> int
{
    return get_plumbing_grid_tracker().storage_at( p ).drain_charges( liquid_type, charges );
}

auto on_contents_changed( const tripoint_abs_ms &p ) -> void
{
    get_plumbing_grid_tracker().invalidate_at( p );
}

auto on_structure_changed( const tripoint_abs_ms &p ) -> void
{
    get_plumbing_grid_tracker().rebuild_at( p );
}

auto disconnect_tank( const tripoint_abs_ms &p ) -> void
{
    get_plumbing_grid_tracker().disconnect_tank_at( p );
}

auto add_grid_connection( const tripoint_abs_omt &lhs, const tripoint_abs_omt &rhs ) -> bool
{
    if( project_to<coords::om>( lhs ).xy() != project_to<coords::om>( rhs ).xy() ) {
        debugmsg( "Connecting plumbing grids on different overmaps is not supported yet" );
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
        debugmsg( "Tried to connect to plumbing grid two points that are connected to each other" );
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
        const auto merged_grid = grid_at( lhs );
        const auto new_anchor = anchor_for_grid( merged_grid );
        auto new_omc = overmap_buffer.get_om_global( new_anchor );
        auto &storage = plumbing_grid::storage_for( *new_omc.om );
        storage.erase( overmap_buffer.get_om_global( anchor_for_grid( lhs_grid ) ).local );
        storage.erase( overmap_buffer.get_om_global( anchor_for_grid( rhs_grid ) ).local );
        storage[new_omc.local] = plumbing_grid::water_storage_state{
            .stored_clean = lhs_state.stored_clean + rhs_state.stored_clean,
            .stored_dirty = lhs_state.stored_dirty + rhs_state.stored_dirty,
            .capacity = lhs_state.capacity + rhs_state.capacity
        };
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
        debugmsg( "Tried to disconnect from plumbing grid two points with no connection to each other" );
        return false;
    }

    const auto old_grid = grid_at( lhs );
    const auto old_state = storage_state_for_grid( old_grid );
    const auto old_anchor = anchor_for_grid( old_grid );

    lhs_bitset[lhs_i] = false;
    rhs_bitset[rhs_i] = false;

    const auto lhs_grid = grid_at( lhs );
    if( !lhs_grid.contains( rhs ) ) {
        const auto rhs_grid = grid_at( rhs );
        const auto lhs_capacity = calculate_capacity_for_grid( lhs_grid, MAPBUFFER );
        const auto rhs_capacity = calculate_capacity_for_grid( rhs_grid, MAPBUFFER );
        const auto split_state = split_storage_state( old_state, lhs_capacity, rhs_capacity );

        auto &storage = plumbing_grid::storage_for( *lhs_omc.om );
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
    plumbing_grid_store().clear();
    plumbing_storage_store().clear();
    get_plumbing_grid_tracker().clear();
}

} // namespace plumbing_grid
