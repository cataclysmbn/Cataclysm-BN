#pragma GCC diagnostic ignored "-Wunused-macros"
#define CATCH_CONFIG_ENABLE_PAIR_STRINGMAKER
#include "catch/catch.hpp"

#include <algorithm>
#include <unordered_map>
#include <vector>
#include <ranges>

#include "cata_algo.h"

static void check_cycle_finding( std::unordered_map<int, std::vector<int>> &g,
                                 std::vector<std::vector<int>> &expected )
{
    CAPTURE( g );
    std::vector<std::vector<int>> loops = cata::find_cycles( g );
    // Canonicalize the list of loops by rotating each to be lexicographically
    // least and then sorting.
    for( std::vector<int> &loop : loops ) {
        auto min = std::ranges::min_element( loop );
        std::ranges::rotate( loop, min );
    }
    std::ranges::sort( loops );
    CHECK( loops == expected );
}

TEST_CASE( "find_cycles_small" )
{
    std::unordered_map<int, std::vector<int>> g = {
        { 0, { 1 } },
        { 1, { 0 } },
    };
    std::vector<std::vector<int>> expected = {
        { 0, 1 },
    };
    check_cycle_finding( g, expected );
}
TEST_CASE( "find_cycles" )
{
    std::unordered_map<int, std::vector<int>> g = {
        { 0, { 0, 1 } },
        { 1, { 0, 2, 3, 17 } },
        { 2, { 1 } },
        { 3, {} },
    };
    std::vector<std::vector<int>> expected = {
        { 0 },
        { 0, 1 },
        { 1, 2 },
    };
    check_cycle_finding( g, expected );
}
TEST_CASE( "flat_map", "[ranges]" )
{
    using namespace cata::ranges;
    auto fn = []( const int i ) -> std::vector<int> { return { i, i * 2 }; };

    SECTION( "general case" ) {
        auto input = std::vector{ 1, 2 };
        auto output = input
                      | flat_map( fn )
                      | std::ranges::to<std::vector>();

        CHECK( output == std::vector{ 1, 2, 2, 4 } );
    }

    SECTION( "empty input" ) {
        auto input = std::vector<int> {};
        auto output = input
                      | flat_map( fn )
                      | std::ranges::to<std::vector>();

        CHECK( output.empty() );
    }
}

struct foo {
    int i;
    auto operator<=>( const foo & ) const = default; // *NOPAD*
};

TEST_CASE( "max", "[ranges]" )
{
    using namespace cata::ranges;

    SECTION( "general case" ) {
        auto input = std::vector{ 1, 2, 3 };
        auto output = input | max();

        CHECK( output == 3 );
    }

    SECTION( "empty input" ) {
        auto input = std::vector<int> {};
        auto output = input | max();

        CHECK( !output.has_value() );
    }
}

TEST_CASE( "max_by", "[ranges]" )
{
    using namespace cata::ranges;

    SECTION( "general case" ) {
        auto input = std::vector{ foo{ 1 }, foo{ 2 }, foo{ 3 } };
        auto output = input | max_by( std::less{} );

        CHECK( output.value() == foo{3} );
    }

    SECTION( "empty input" ) {
        auto input = std::vector<int> {};
        auto output = input | max_by( std::less{} );

        CHECK( !output.has_value() );
    }
}

TEST_CASE( "min", "[ranges]" )
{
    using namespace cata::ranges;

    SECTION( "general case" ) {
        auto input = std::vector{ 1, 2, 3 };
        auto output = input | min();

        CHECK( output == 1 );
    }

    SECTION( "empty input" ) {
        auto input = std::vector<int> {};
        auto output = input | min();

        CHECK( !output.has_value() );
    }
}
TEST_CASE( "min_by", "[ranges]" )
{
    using namespace cata::ranges;

    SECTION( "general case" ) {
        auto input = std::vector{ foo{ 1 }, foo{ 2 }, foo{ 3 } };
        auto output = input | min_by( std::less{} );

        CHECK( output.value() == foo{1} );
    }

    SECTION( "empty input" ) {
        auto input = std::vector<int> {};
        auto output = input | min_by( std::less{} );

        CHECK( !output.has_value() );
    }
}

TEST_CASE( "complex pipeline", "[ranges]" )
{
    using namespace cata::ranges;

    auto dup = []( const int i ) -> int { return i * 2; };
    auto is_even = []( const int i ) -> bool { return i % 2 == 0; };
    auto input = std::vector{ 1, 2, 3 };
    auto output = input
                  | std::views::filter( is_even )
                  | flat_map( []( const int i ) -> auto { return std::vector{i, i * 2 }; } )
                  | std::views::transform( dup )
                  | max();

    CHECK( output == 8 );
}
