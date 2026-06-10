#include "catch/catch.hpp"

#include <chrono>
#include <cstddef>
#include <functional>
#include <latch>
#include <optional>
#include <thread>
#include <vector>

#include "test_statistics.h"
#include "random/rng.h"
#include "thread_pool.h"

static void check_remainder( float proportion )
{
    statistics<float> stats;
    const epsilon_threshold target_range{ proportion, 0.05 };
    do {
        stats.add( roll_remainder( proportion ) );
    } while( stats.n() < 100 || stats.uncertain_about( target_range ) );
    INFO( "Goal: " << proportion );
    INFO( "Result: " << stats.avg() );
    INFO( "Samples: " << stats.n() );
    CHECK( stats.test_threshold( target_range ) );
}

TEST_CASE( "roll_remainder_distribution", "[.]" )
{
    check_remainder( 0.0 );
    check_remainder( 0.01 );
    check_remainder( -0.01 );
    check_remainder( 1.5 );
    check_remainder( -1.5 );
    check_remainder( 1.75 );
    check_remainder( -1.75 );
    check_remainder( 1.1 );
    check_remainder( -1.1 );
    check_remainder( 1.0 );
    check_remainder( -1.0 );
    check_remainder( 10.0 );
    check_remainder( -10.0 );
    check_remainder( 10.5 );
    check_remainder( -10.5 );
}

static void check_x_in_y( double x, double y )
{
    statistics<bool> stats( Z99_999_9 );
    const epsilon_threshold target_range{ x / y, 0.05 };
    do {
        stats.add( x_in_y( x, y ) );
    } while( stats.n() < 100 || stats.uncertain_about( target_range ) );
    INFO( "Goal: " << x << " / " << y << " ( " << x / y << " )" );
    INFO( "Result: " << stats.avg() );
    INFO( "Samples: " << stats.n() );
    CHECK( stats.test_threshold( target_range ) );
}

TEST_CASE( "x_in_y_distribution", "[.]" )
{
    float y_increment = 0.01;
    for( float y = 0.1; y < 500.0; y += y_increment ) {
        y_increment *= 1.1;
        float x_increment = 0.1;
        // NOLINTNEXTLINE(clang-analyzer-security.FloatLoopCounter)
        for( float x = 0.1; x < y; x += x_increment ) {
            check_x_in_y( x, y );
            x_increment *= 1.1;
        }
    }
}

TEST_CASE( "random_entry_preserves_constness" )
{
    const std::vector<int> v0{ 4321 };
    int i0 = *random_entry_opt( v0 );
    CHECK( i0 == 4321 );

    std::vector<int> v1{ 1234 };
    int &i1 = *random_entry_opt( v1 );
    CHECK( i1 == 1234 );
    i1 = 5678;
    CHECK( v1[0] == 5678 );
}

TEST_CASE( "deterministic_rng_scopes_parallel_tasks", "[rng][thread_pool]" )
{
    const auto run_submitted_tasks = []() {
        rng_set_deterministic_seed( 424242 );
        auto values = std::vector<int>( 16 );
        auto done = std::latch( static_cast<std::ptrdiff_t>( values.size() ) );
        auto pool = cata_thread_pool( 4 );
        for( auto i = std::size_t{ 0 }; i < values.size(); ++i ) {
            pool.submit( { .stream = 0x746573745f737562ULL, .id = i },
            [&values, &done, i]() {
                if( i % 2 == 0 ) {
                    std::this_thread::sleep_for( std::chrono::milliseconds( 1 ) );
                }
                values[i] = rng( 1, 1000000 );
                done.count_down();
            } );
        }
        done.wait();
        return values;
    };

    const auto run_parallel_for = []() {
        rng_set_deterministic_seed( 424242 );
        auto values = std::vector<int>( 16 );
        parallel_for( 0, static_cast<int>( values.size() ), [&values]( const int i ) {
            if( i % 2 == 0 ) {
                std::this_thread::sleep_for( std::chrono::milliseconds( 1 ) );
            }
            values[i] = rng( 1, 1000000 );
        } );
        return values;
    };

    const auto first_submitted = run_submitted_tasks();
    const auto second_submitted = run_submitted_tasks();
    const auto first_parallel = run_parallel_for();
    const auto second_parallel = run_parallel_for();
    rng_clear_deterministic_seed();
    CHECK( first_submitted == second_submitted );
    CHECK( first_parallel == second_parallel );
}
