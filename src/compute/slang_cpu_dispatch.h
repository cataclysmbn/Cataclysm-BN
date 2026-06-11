#pragma once

#include "thread_pool.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <ranges>
#include <span>
#include <vector>

namespace cata_compute::slang_cpu::kernels
{

struct cpu_dispatch_group {
    uint32_t x = 0;
    uint32_t y = 0;
    uint32_t z = 0;
};

struct cpu_dispatch_range {
    cpu_dispatch_group start = {};
    cpu_dispatch_group end = {};
};

struct independent_dispatch_grid {
    uint32_t group_x = 0;
    uint32_t group_y = 1;
    uint32_t group_z = 1;
    uint32_t min_groups_per_chunk = 4;
};

struct accumulating_dispatch_grid {
    uint32_t group_x = 0;
    uint32_t group_y = 1;
    uint32_t group_z = 1;
    uint32_t output_values = 0;
    uint32_t min_groups_per_chunk = 4;
    uint32_t max_chunks = 4;
    uint64_t max_scratch_bytes = 96ULL * 1024ULL * 1024ULL;
};

#if defined( CATA_SLANG_CPU_GENERATED )
inline auto make_varying( cpu_dispatch_range const &range ) -> ComputeVaryingInput
{
    return {
        .startGroupID = uint3( range.start.x, range.start.y, range.start.z ),
        .endGroupID = uint3( range.end.x, range.end.y, range.end.z ),
    };
}
#endif

template<typename RunRange>
auto dispatch_independent( independent_dispatch_grid const &grid, RunRange &&run_range ) -> void
{
    if( grid.group_x == 0 || grid.group_y == 0 || grid.group_z == 0 ) {
        return;
    }

    const auto run_full = [&]() {
        run_range( cpu_dispatch_range {
            .start = {},
            .end = {
                .x = grid.group_x,
                .y = grid.group_y,
                .z = grid.group_z,
            },
        } );
    };

    if( grid.group_x < grid.min_groups_per_chunk || is_pool_worker_thread() ) {
        run_full();
        return;
    }

    auto &pool = get_thread_pool();
    const auto worker_count = pool.num_workers();
    if( worker_count == 0 ) {
        run_full();
        return;
    }

    const auto chunks_by_size =
        ( grid.group_x + grid.min_groups_per_chunk - 1 ) / grid.min_groups_per_chunk;
    const auto chunks_by_workers = worker_count * 4U;
    const auto chunk_count = std::min( { grid.group_x, chunks_by_size, chunks_by_workers } );
    if( chunk_count <= 1 ) {
        run_full();
        return;
    }

    parallel_for( 0, static_cast<int>( chunk_count ), [&]( const auto chunk_index ) {
        const auto chunk = static_cast<uint32_t>( chunk_index );
        const auto start_x = grid.group_x * chunk / chunk_count;
        const auto end_x = grid.group_x * ( chunk + 1U ) / chunk_count;
        if( start_x == end_x ) {
            return;
        }
        run_range( cpu_dispatch_range {
            .start = {
                .x = start_x,
                .y = 0,
                .z = 0,
            },
            .end = {
                .x = end_x,
                .y = grid.group_y,
                .z = grid.group_z,
            },
        } );
    } );
}

inline auto make_accumulating_ranges( accumulating_dispatch_grid const &grid )
-> std::vector<cpu_dispatch_range>
{
    auto ranges = std::vector<cpu_dispatch_range> {};
    if( grid.group_x == 0 || grid.group_y == 0 || grid.group_z == 0 ) {
        return ranges;
    }

    const auto full_range = cpu_dispatch_range {
        .start = {},
        .end = {
            .x = grid.group_x,
            .y = grid.group_y,
            .z = grid.group_z,
        },
    };

    const auto push_full = [&]() {
        ranges.push_back( full_range );
    };

    if( grid.group_x < grid.min_groups_per_chunk || grid.max_chunks <= 1 ||
        is_pool_worker_thread() ) {
        push_full();
        return ranges;
    }

    auto &pool = get_thread_pool();
    auto chunk_limit = std::min( grid.max_chunks, pool.num_workers() );
    if( grid.output_values > 0 && grid.max_scratch_bytes > 0 ) {
        const auto bytes_per_buffer = static_cast<uint64_t>( grid.output_values ) *
                                      static_cast<uint64_t>( sizeof( uint32_t ) );
        if( bytes_per_buffer > 0 ) {
            const auto memory_limited_chunks = static_cast<uint32_t>(
                                                   grid.max_scratch_bytes / bytes_per_buffer );
            chunk_limit = std::min( chunk_limit, memory_limited_chunks );
        }
    }
    if( chunk_limit <= 1 ) {
        push_full();
        return ranges;
    }

    const auto chunks_by_size =
        ( grid.group_x + grid.min_groups_per_chunk - 1U ) / grid.min_groups_per_chunk;
    const auto chunk_count = std::min( { grid.group_x, chunks_by_size, chunk_limit } );
    if( chunk_count <= 1 ) {
        push_full();
        return ranges;
    }

    ranges.reserve( chunk_count );
    for( const auto chunk : std::views::iota( 0U, chunk_count ) ) {
        const auto start_x = grid.group_x * chunk / chunk_count;
        const auto end_x = grid.group_x * ( chunk + 1U ) / chunk_count;
        if( start_x == end_x ) {
            continue;
        }
        ranges.push_back( cpu_dispatch_range {
            .start = {
                .x = start_x,
                .y = 0,
                .z = 0,
            },
            .end = {
                .x = end_x,
                .y = grid.group_y,
                .z = grid.group_z,
            },
        } );
    }
    if( ranges.empty() ) {
        push_full();
    }
    return ranges;
}

inline auto make_zero_uint_buffers( const std::size_t buffer_count, const uint32_t value_count )
-> std::vector<std::vector<uint32_t>>
{
    auto buffers = std::vector<std::vector<uint32_t>> {};
    buffers.reserve( buffer_count );
    for( const auto _ : std::views::iota( std::size_t { 0 }, buffer_count ) ) {
        ( void )_;
        buffers.emplace_back( value_count, 0U );
    }
    return buffers;
}

template<typename RunChunk>
auto dispatch_accumulating_chunks( std::span<cpu_dispatch_range const> ranges,
                                   RunChunk &&run_chunk ) -> void
{
    if( ranges.empty() ) {
        return;
    }
    if( ranges.size() == 1 ) {
        run_chunk( std::size_t { 0 }, ranges.front() );
        return;
    }

    parallel_for( 0, static_cast<int>( ranges.size() ), [&]( const auto chunk_index ) {
        const auto index = static_cast<std::size_t>( chunk_index );
        run_chunk( index, ranges[index] );
    } );
}

} // namespace cata_compute::slang_cpu::kernels
