#include "catch/catch.hpp"

#include "data_vars.h"

#include <complex>

TEST_CASE( "load_store_string", "[data_vars]" )
{
    data_vars::data_set dv;
    CHECK( dv.empty() );

    dv.set( "string1", "1234" );
    CHECK( dv["string1"] == "1234" );

    CHECK( dv.get( "string2", "default" ) == "default" );

    CHECK( dv.get( "string3", "" ) == "" );

    dv.erase( "string1" );
    CHECK_FALSE( dv.contains( "string1" ) );

    dv.clear();
    CHECK( dv.empty() );

    dv.set( "string", ",[]\"" );
    CHECK( dv["string"] == ",[]\"" );

}

TEST_CASE( "load_store_numbers", "[data_vars]" )
{
    data_vars::data_set dv;
    CHECK( dv.empty() );

    for( int i = 0; i < 1000; i++ ) {

        auto x = rand();

        dv.set( "value", x );
        CHECK( dv.contains( "value" ) );

        auto i4 = dv.get<int32_t>( "value" );
        CHECK( x == i4 );

        auto i8 = dv.get<int64_t>( "value" );
        CHECK( x == i8 );

        auto f4 = dv.get<float>( "value" );
        CHECK( x == f4 );

        auto f8 = dv.get<double>( "value" );
        CHECK( x == f8 );
    }
}

TEST_CASE( "load_store_points", "[data_vars]" )
{
    data_vars::data_set dv;
    CHECK( dv.empty() );

    for( int i = 0; i < 1000; i++ ) {
        auto key = std::to_string( rand() % 10 );
        point p = { rand(), rand() };

        dv.set( key, p );

        point q = dv.get<point>( key );

        CHECK( p == q );
    }
}

TEST_CASE( "load_store_tripoints", "[data_vars]" )
{
    data_vars::data_set dv;
    CHECK( dv.empty() );

    for( int i = 0; i < 1000; i++ ) {
        auto key = std::to_string( rand() % 10 );
        tripoint p = { rand(), rand(), rand() };

        dv.set( key, p );

        tripoint q = dv.get<tripoint>( key );

        CHECK( p == q );
    }
}

template<typename T, typename F, size_t ... Is>
static auto gen_array( F &&fun, std::index_sequence<Is...> )
{
    auto rand = [&]( size_t i ) { return fun( i ); };
    return std::array{ rand( Is ) ... };
}

TEST_CASE( "load_store_numeric_arrays", "[data_vars]" )
{
    data_vars::data_set dv;
    CHECK( dv.empty() );

    std::random_device rd;
    std::mt19937 gen( rd() );
    std::uniform_int_distribution<int> distrib( 0, 100000 );

    for( int i = 0; i < 1000; i++ ) {
        auto key = std::to_string( rand() % 10 );
        auto fn = [&]( size_t ) { return distrib( gen ); };
        auto p = gen_array<int>( fn, std::make_index_sequence<10>() );
        dv.set( key, p );
        auto q = dv.get<decltype( p )>( key );
        CHECK( std::ranges::equal( p, q, []( auto & lhs, auto & rhs ) { return std::fabs( lhs - rhs ) == 0; } ) );
    }

    std::uniform_int_distribution<int> distrib2( 0, 10 );

    for( int i = 0; i < 1000; i++ ) {
        auto key = std::to_string( rand() % 10 );
        auto p = std::vector<int>();
        for( int j = 0; j < distrib2( gen ); j++ ) {
            p.push_back( distrib( gen ) );
        }
        dv.set( key, p );
        auto q = dv.get<decltype( p )>( key );
        CHECK( std::ranges::equal( p, q, []( auto & lhs, auto & rhs ) { return std::fabs( lhs - rhs ) == 0; } ) );
    }
}

TEST_CASE( "load_store_string_arrays", "[data_vars]" )
{
    data_vars::data_set dv;
    CHECK( dv.empty() );

    {
        auto key = std::to_string( rand() % 10 );
        auto p = gen_array<std::string>( []( size_t i ) { return "item" + std::to_string( i ); },
        std::make_index_sequence<10>() );
        dv.set( key, p );
        auto q = dv.get<decltype( p )>( key );
        CHECK( p == q );
    }

    {
        auto key = std::to_string( rand() % 10 );
        auto p = std::vector<std::string> {",", "[", "]", ",", "\"", "[]\",", "{\"key\":1213}" };
        dv.set( key, p );
        auto q = dv.get<decltype( p )>( key );
        CHECK( p == q );
    }
}

TEST_CASE( "load_store_map", "[data_vars]" )
{
    data_vars::data_set dv;
    CHECK( dv.empty() );

    std::random_device rd;
    std::mt19937 gen( rd() );
    std::uniform_int_distribution<int> distrib( 0, 100000 );

    {
        std::map<std::string, int> p;
        for( int i = 0; i < 10; i++ ) {
            auto key = std::to_string( i );
            auto value = distrib( gen );
            p[key] = value;
        }
        dv.set( "test", p );
        auto q = dv.get<decltype( p )>( "test" );
        CHECK( p == q );
    }
}

TEST_CASE( "load_store_string_ids", "[data_vars]" )
{
    data_vars::data_set dv;
    CHECK( dv.empty() );

    std::random_device rd;
    std::mt19937 gen( rd() );
    std::uniform_int_distribution<int> distrib( 0, 100000 );

    struct test_obj {};

    constexpr int num_ids = 1000000;

    std::vector<string_id<test_obj>> ids;
    // lots of ids to make sure that "interning" map gets expanded
    for( int i = 0; i < num_ids; ++i ) {
        ids.push_back( string_id<test_obj>( "test_id" + std::to_string( i ) ) );
    }

    {
        dv.set( "test", ids );
        auto q = dv.get<decltype( ids )>( "test" );
        CHECK( ids == q );
    }
}