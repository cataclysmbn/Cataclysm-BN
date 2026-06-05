#include "catch/catch.hpp"

#include <atomic>
#include <ranges>
#include <thread>
#include <vector>

#include "cata_arena.h"
#include "item.h"

TEST_CASE( "item arena cleanup tolerates concurrent destruction", "[arena]" )
{
    constexpr auto worker_count = 8;
    constexpr auto items_per_worker = 5000;

    while( cata_arena<item>::cleanup() ) {}

    auto ready = std::atomic<int> { 0 };
    auto finished = std::atomic<int> { 0 };
    auto start = std::atomic<bool> { false };
    auto workers = std::vector<std::thread> {};

    for( const auto worker_index : std::views::iota( 0, worker_count ) ) {
        workers.emplace_back( [&ready, &finished, &start, worker_index, items_per_worker]() {
            ready.fetch_add( 1, std::memory_order_release );
            while( !start.load( std::memory_order_acquire ) ) {
                std::this_thread::yield();
            }
            for( const auto item_index : std::views::iota( 0, items_per_worker ) ) {
                cata_arena<item>::mark_for_destruction( new item() );
                if( ( item_index + worker_index ) % 8 == 0 ) {
                    std::this_thread::yield();
                }
            }
            finished.fetch_add( 1, std::memory_order_release );
        } );
    }

    while( ready.load( std::memory_order_acquire ) != worker_count ) {
        std::this_thread::yield();
    }
    start.store( true, std::memory_order_release );

    while( finished.load( std::memory_order_acquire ) != worker_count ) {
        cata_arena<item>::cleanup();
        std::this_thread::yield();
    }

    for( auto &worker : workers ) {
        worker.join();
    }
    while( cata_arena<item>::cleanup() ) {}

    SUCCEED( "concurrent item destruction and arena cleanup completed" );
}
