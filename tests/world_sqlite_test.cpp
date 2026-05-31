#include "catch/catch.hpp"

#include <algorithm>
#include <atomic>
#include <ostream>
#include <ranges>
#include <stdexcept>

#include "coordinates.h"
#include "filesystem.h"
#include "game.h"
#include "thread_pool.h"
#include "world.h"

namespace
{

auto concurrent_test_omt( const int index ) -> tripoint_abs_omt { return { 10000 + index, 20000 + index, 0 }; }

} // namespace

TEST_CASE( "sqlite map database accepts concurrent map writes", "[world][sqlite]" )
{
    auto *const w = g->get_active_world();
    REQUIRE( w != nullptr );
    REQUIRE( w->info->world_save_format == save_format::V2_COMPRESSED_SQLITE3 );

    static constexpr auto write_count = 64;
    const auto dim_id = "sqlite_concurrent_" + get_pid_string();

    parallel_for( 0, write_count, [&]( const auto i ) {
        const auto omt_addr = concurrent_test_omt( i );
        if( !w->write_map_omt( dim_id, omt_addr, []( std::ostream & out ) { out << "[]"; } ) ) {
            throw std::runtime_error( "failed to write sqlite map omt" );
        }
    } );

    const auto all_written = std::ranges::all_of( std::views::iota( 0, write_count ),
    [&]( const auto i ) {
        const auto omt_addr = concurrent_test_omt( i );
        return w->read_map_omt( dim_id, omt_addr, []( JsonIn & jsin ) {
            jsin.start_array();
            jsin.end_array();
        } );
    } );
    CHECK( all_written );
}

TEST_CASE( "sqlite map database accepts concurrent map reads", "[world][sqlite]" )
{
    auto *const w = g->get_active_world();
    REQUIRE( w != nullptr );
    REQUIRE( w->info->world_save_format == save_format::V2_COMPRESSED_SQLITE3 );

    static constexpr auto read_count = 64;
    const auto dim_id = "sqlite_concurrent_reads_" + get_pid_string();

    std::ranges::for_each( std::views::iota( 0, read_count ), [&]( const auto i ) {
        const auto omt_addr = concurrent_test_omt( i + read_count );
        REQUIRE( w->write_map_omt( dim_id, omt_addr, []( std::ostream & out ) { out << "[]"; } ) );
    } );

    auto all_read = std::atomic_bool{ true };
    const auto read_empty_array = []( JsonIn & jsin ) {
        jsin.start_array();
        jsin.end_array();
    };
    parallel_for( 0, read_count, [&]( const auto i ) {
        const auto omt_addr = concurrent_test_omt( i + read_count );
        if( !w->read_map_omt( dim_id, omt_addr, read_empty_array ) ) {
            all_read.store( false, std::memory_order_relaxed );
        }
    } );

    CHECK( all_read.load( std::memory_order_relaxed ) );
}
