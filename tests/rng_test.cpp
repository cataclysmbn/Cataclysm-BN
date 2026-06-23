#include "catch/catch.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <latch>
#include <random>
#include <vector>

#include "cata_utility.h"
#include "test_statistics.h"
#include "random/rng.h"
#include "thread_pool.h"

namespace
{

auto check_remainder( float proportion ) -> void
{
    auto stats = statistics<float> {};
    const auto target_range = epsilon_threshold{ proportion, 0.05 };
    do {
        stats.add( roll_remainder( proportion ) );
    } while( stats.n() < 100 || stats.uncertain_about( target_range ) );
    INFO( "Goal: " << proportion );
    INFO( "Result: " << stats.avg() );
    INFO( "Samples: " << stats.n() );
    CHECK( stats.test_threshold( target_range ) );
}

} // namespace

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

namespace
{

auto check_x_in_y( double x, double y ) -> void
{
    auto stats = statistics<bool>( Z99_999_9 );
    const auto target_range = epsilon_threshold{ x / y, 0.05 };
    do {
        stats.add( x_in_y( x, y ) );
    } while( stats.n() < 100 || stats.uncertain_about( target_range ) );
    INFO( "Goal: " << x << " / " << y << " ( " << x / y << " )" );
    INFO( "Result: " << stats.avg() );
    INFO( "Samples: " << stats.n() );
    CHECK( stats.test_threshold( target_range ) );
}

} // namespace

TEST_CASE( "x_in_y_distribution", "[.]" )
{
    auto y_increment = 0.01f;
    for( auto y = 0.1f; y < 500.0f; y += y_increment ) {
        y_increment *= 1.1f;
        auto x_increment = 0.1f;
        // NOLINTNEXTLINE(clang-analyzer-security.FloatLoopCounter)
        for( auto x = 0.1f; x < y; x += x_increment ) {
            check_x_in_y( x, y );
            x_increment *= 1.1;
        }
    }
}

TEST_CASE( "random_entry_preserves_constness" )
{
    const auto v0 = std::vector<int> { 4321 };
    auto i0 = random_entry_opt( v0 );
    REQUIRE( i0 );
    CHECK( i0->get() == 4321 );

    auto v1 = std::vector<int> { 1234 };
    auto i1 = random_entry_opt( v1 );
    REQUIRE( i1 );
    CHECK( i1->get() == 1234 );
    i1->get() = 5678;
    CHECK( v1[0] == 5678 );
}

TEST_CASE( "deterministic_rng_inactive_until_seeded", "[rng][thread_pool]" )
{
    rng_clear_deterministic_seed();
    const auto cleanup = on_out_of_scope( []() { rng_clear_deterministic_seed(); } );

    CHECK( !rng_deterministic_seed_active() );
    CHECK( !rng_deterministic_seed_for_current_context( { .stream = 1, .id = 2 } ) );
    CHECK( !rng_next_deterministic_call_seed( 3 ) );
}

TEST_CASE( "deterministic_rng_normalizes_zero_seed", "[rng][thread_pool]" )
{
    const auto cleanup = on_out_of_scope( []() { rng_clear_deterministic_seed(); } );

    rng_set_deterministic_seed( 0 );
    const auto zero_seed_roll = rng( 1, 1000000 );
    const auto zero_seed_key = rng_deterministic_seed_for( { .stream = 4, .id = 5 } );

    rng_clear_deterministic_seed();
    rng_set_deterministic_seed( 1 );
    CHECK( rng( 1, 1000000 ) == zero_seed_roll );
    CHECK( rng_deterministic_seed_for( { .stream = 4, .id = 5 } ) == zero_seed_key );
    CHECK( zero_seed_key != 0 );
}

TEST_CASE( "deterministic_rng_clear_restores_main_engine", "[rng][thread_pool]" )
{
    rng_clear_deterministic_seed();
    rng_set_engine_seed( 13579 );
    const auto expected_next_roll = rng( 1, 1000000 );

    rng_set_engine_seed( 13579 );
    rng_set_deterministic_seed( 24680 );
    ( void )rng( 1, 1000000 );
    rng_clear_deterministic_seed();

    CHECK( rng( 1, 1000000 ) == expected_next_roll );
}

TEST_CASE( "deterministic_rng_normal_roll_uses_scope_engine", "[rng][thread_pool]" )
{
    rng_set_deterministic_seed( 97531 );
    const auto cleanup = on_out_of_scope( []() { rng_clear_deterministic_seed(); } );
    const auto first_seed = rng_deterministic_seed_for( { .stream = 11, .id = 12 } );
    const auto second_seed = rng_deterministic_seed_for( { .stream = 13, .id = 14 } );
    auto expected_engine = cata_default_random_engine( second_seed );
    auto expected_distribution = std::normal_distribution<double> {};
    const auto expected_second = expected_distribution( expected_engine,
                                 std::normal_distribution<>::param_type( 10.0, 2.0 ) );

    {
        const auto first_scope = rng_deterministic_task_scope( first_seed );
        ( void )normal_roll( 10.0, 2.0 );
    }
    {
        const auto second_scope = rng_deterministic_task_scope( second_seed );
        CHECK( normal_roll( 10.0, 2.0 ) == expected_second );
    }
}

TEST_CASE( "deterministic_rng_task_scope_restores_nested_child_counter", "[rng][thread_pool]" )
{
    rng_set_deterministic_seed( 31337 );
    const auto cleanup = on_out_of_scope( []() { rng_clear_deterministic_seed(); } );
    constexpr auto stream = std::uint64_t{ 0x6e65737465645f5fULL };
    const auto outer_seed = rng_deterministic_seed_for( { .stream = 1, .id = 2 } );

    {
        const auto outer_scope = rng_deterministic_task_scope( outer_seed );
        const auto first_child = rng_next_deterministic_call_seed( stream );
        const auto second_child = rng_next_deterministic_call_seed( stream );
        REQUIRE( first_child );
        REQUIRE( second_child );
        CHECK( *first_child == rng_deterministic_child_seed( outer_seed,
        { .stream = stream, .id = 0 } ) );
        CHECK( *second_child == rng_deterministic_child_seed( outer_seed,
        { .stream = stream, .id = 1 } ) );

        {
            const auto nested_scope = rng_deterministic_task_scope( 1234 );
            const auto nested_child = rng_next_deterministic_call_seed( stream );
            REQUIRE( nested_child );
            CHECK( *nested_child == rng_deterministic_child_seed( 1234,
            { .stream = stream, .id = 0 } ) );
        }

        const auto after_nested = rng_next_deterministic_call_seed( stream );
        REQUIRE( after_nested );
        CHECK( *after_nested == rng_deterministic_child_seed( outer_seed,
        { .stream = stream, .id = 2 } ) );
    }

    CHECK( rng_next_deterministic_call_seed( stream ) );
}

TEST_CASE( "deterministic_rng_scopes_parallel_tasks", "[rng][thread_pool]" )
{
    const auto cleanup = on_out_of_scope( []() { rng_clear_deterministic_seed(); } );

    const auto run_submitted_tasks = []() {
        rng_set_deterministic_seed( 424242 );
        auto values = std::vector<int>( 16 );
        auto done = std::latch( static_cast<std::ptrdiff_t>( values.size() ) );
        auto pool = cata_thread_pool( 4 );
        for( auto i = std::size_t{ 0 }; i < values.size(); ++i ) {
            pool.submit( { .stream = 0x746573745f737562ULL, .id = i },
            [&values, &done, i]() {
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
            values[i] = rng( 1, 1000000 );
        } );
        return values;
    };

    const auto first_submitted = run_submitted_tasks();
    const auto second_submitted = run_submitted_tasks();
    const auto first_parallel = run_parallel_for();
    const auto second_parallel = run_parallel_for();
    CHECK( first_submitted == second_submitted );
    CHECK( first_parallel == second_parallel );
}

TEST_CASE( "deterministic_rng_keyed_tasks_ignore_submission_order", "[rng][thread_pool]" )
{
    const auto run_order = []( const std::vector<std::size_t> &order ) {
        rng_set_deterministic_seed( 5150 );
        const auto cleanup = on_out_of_scope( []() { rng_clear_deterministic_seed(); } );
        auto values = std::vector<int>( order.size() );
        auto done = std::latch( static_cast<std::ptrdiff_t>( order.size() ) );
        auto pool = cata_thread_pool( 4 );
        for( const auto i : order ) {
            pool.submit( { .stream = 0x6f726465725f5f5fULL, .id = i },
            [&values, &done, i]() {
                values[i] = rng( 1, 1000000 );
                done.count_down();
            } );
        }
        done.wait();
        return values;
    };

    CHECK( run_order( { 0, 1, 2, 3, 4, 5, 6, 7 } ) ==
           run_order( { 7, 6, 5, 4, 3, 2, 1, 0 } ) );
}

TEST_CASE( "deterministic_rng_scopes_submit_returning_without_workers", "[rng][thread_pool]" )
{
    rng_set_deterministic_seed( 600613 );
    const auto cleanup = on_out_of_scope( []() { rng_clear_deterministic_seed(); } );
    auto pool = cata_thread_pool( 0 );

    const auto first = pool.submit_returning( { .stream = 0x73796e635f5f5f5fULL, .id = 42 }, []() {
        return std::array<int, 3> { rng( 1, 1000000 ), rng( 1, 1000000 ), rng( 1, 1000000 ) };
    } ).get();
    rng( 1, 1000000 );
    const auto second = pool.submit_returning( { .stream = 0x73796e635f5f5f5fULL, .id = 42 }, []() {
        return std::array<int, 3> { rng( 1, 1000000 ), rng( 1, 1000000 ), rng( 1, 1000000 ) };
    } ).get();

    CHECK( first == second );
}

TEST_CASE( "deterministic_rng_scopes_parallel_for_chunked", "[rng][thread_pool]" )
{
    const auto run_chunked = []() {
        rng_set_deterministic_seed( 2468 );
        const auto cleanup = on_out_of_scope( []() { rng_clear_deterministic_seed(); } );
        auto values = std::vector<int>( 24 );
        parallel_for_chunked( 0, static_cast<int>( values.size() ), 3, [&values]( const int i ) {
            values[i] = rng( 1, 1000000 );
        } );
        return values;
    };

    CHECK( run_chunked() == run_chunked() );
}
