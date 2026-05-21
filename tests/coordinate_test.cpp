#include "catch/catch.hpp"

#include "coordinates.h"
#include "cata_generators.h"
#include "stringmaker.h"

constexpr int num_trials = 5;

static_assert( point::dimension == 2, "" );
static_assert( tripoint::dimension == 3, "" );
static_assert( point_abs_omt::dimension == 2, "" );
static_assert( tripoint_abs_omt::dimension == 3, "" );

TEST_CASE( "coordinate_strings", "[point][coords]" )
{
    CHECK( point_abs_omt( point( 3, 4 ) ).to_string() == "(3,4)" );

    SECTION( "coord_point_matches_point" ) {
        point p = GENERATE( take( num_trials, random_points() ) );
        point_abs_ms cp( p );
        CHECK( p.to_string() == cp.to_string() );
    }
}

TEST_CASE( "coordinate_operations", "[point][coords]" )
{
    SECTION( "construct_from_raw_point" ) {
        point p = GENERATE( take( num_trials, random_points() ) );
        point_abs_ms cp( p );
        CHECK( cp.x() == p.x );
        CHECK( cp.y() == p.y );
    }

    SECTION( "construct_from_raw_tripoint" ) {
        tripoint p = GENERATE( take( num_trials, random_tripoints() ) );
        tripoint_abs_ms cp( p );
        CHECK( cp.x() == p.x );
        CHECK( cp.y() == p.y );
        CHECK( cp.z() == p.z );
    }

    SECTION( "construct_from_values" ) {
        tripoint p = GENERATE( take( num_trials, random_tripoints() ) );
        {
            point_abs_ms cp( p.x, p.y );
            CHECK( cp.x() == p.x );
            CHECK( cp.y() == p.y );
        }
        {
            tripoint_abs_ms cp( p.x, p.y, p.z );
            CHECK( cp.x() == p.x );
            CHECK( cp.y() == p.y );
            CHECK( cp.z() == p.z );
        }
    }

    SECTION( "addition" ) {
        tripoint t0 = GENERATE( take( num_trials, random_tripoints() ) );
        point p0 = t0.xy();
        point p1 = GENERATE( take( num_trials, random_points() ) );
        CAPTURE( p0, p1 );
        tripoint_abs_ms abst0( t0 );
        point_abs_ms abs0( p0 );
        point_rel_ms rel0( p0 );
        point_rel_ms rel1( p1 );
        SECTION( "rel + rel -> rel" ) {
            point_rel_ms sum = rel0 + rel1;
            CHECK( sum.raw() == p0 + p1 );
        }
        SECTION( "abs + rel -> abs" ) {
            point_abs_ms sum = abs0 + rel1;
            CHECK( sum.raw() == p0 + p1 );
            tripoint_abs_ms sum_t = abst0 + rel1;
            CHECK( sum_t.raw() == t0 + p1 );
        }
        SECTION( "abs + raw -> abs" ) {
            point_abs_ms sum = abs0 + p1;
            CHECK( sum.raw() == p0 + p1 );
        }
        SECTION( "rel + abs -> abs" ) {
            point_abs_ms sum = rel1 + abs0;
            CHECK( sum.raw() == p0 + p1 );
        }
        SECTION( "rel += rel" ) {
            rel0 += rel1;
            CHECK( rel0.raw() == p0 + p1 );
        }
        SECTION( "abs += rel" ) {
            abs0 += rel1;
            CHECK( abs0.raw() == p0 + p1 );
        }
        SECTION( "abs += raw" ) {
            abs0 += p1;
            CHECK( abs0.raw() == p0 + p1 );
        }
    }

    SECTION( "subtraction" ) {
        tripoint t0 = GENERATE( take( num_trials, random_tripoints() ) );
        point p0 = t0.xy();
        point p1 = GENERATE( take( num_trials, random_points() ) );
        CAPTURE( p0, p1 );
        tripoint_abs_ms abst0( t0 );
        point_abs_ms abs0( p0 );
        point_abs_ms abs1( p1 );
        point_rel_ms rel0( p0 );
        point_rel_ms rel1( p1 );
        SECTION( "rel - rel -> rel" ) {
            point_rel_ms diff = rel0 - rel1;
            CHECK( diff.raw() == p0 - p1 );
        }
        SECTION( "abs - rel -> abs" ) {
            point_abs_ms diff = abs0 - rel1;
            CHECK( diff.raw() == p0 - p1 );
        }
        SECTION( "abs - raw -> abs" ) {
            point_abs_ms diff = abs0 - p1;
            CHECK( diff.raw() == p0 - p1 );
        }
        SECTION( "abs - abs -> rel" ) {
            point_rel_ms diff0 = abs0 - abs1;
            CHECK( diff0.raw() == p0 - p1 );
            tripoint_rel_ms diff1 = abst0 - abs1;
            CHECK( diff1.raw() == t0 - p1 );
        }
        SECTION( "rel -= rel" ) {
            rel0 -= rel1;
            CHECK( rel0.raw() == p0 - p1 );
        }
        SECTION( "abs -= rel" ) {
            abs0 -= rel1;
            CHECK( abs0.raw() == p0 - p1 );
        }
        SECTION( "abs -= raw" ) {
            abs0 -= p1;
            CHECK( abs0.raw() == p0 - p1 );
        }
    }
}

TEST_CASE( "coordinate_comparison", "[point][coords]" )
{
    SECTION( "compare_points" ) {
        point p0 = GENERATE( take( num_trials, random_points() ) );
        point p1 = GENERATE( take( num_trials, random_points() ) );
        CAPTURE( p0, p1 );
        point_rel_ms cp0( p0 );
        point_rel_ms cp1( p1 );
        CAPTURE( cp0, cp1 );

        CHECK( ( p0 < p1 ) == ( cp0 < cp1 ) );
        CHECK( ( p0 == p1 ) == ( cp0 == cp1 ) );
        CHECK( cp0 == cp0 );
        CHECK( !( cp0 != cp0 ) );
    }

    SECTION( "compare_tripoints" ) {
        tripoint p0 = GENERATE( take( num_trials, random_tripoints() ) );
        tripoint p1 = GENERATE( take( num_trials, random_tripoints() ) );
        CAPTURE( p0, p1 );
        tripoint_rel_ms cp0( p0 );
        tripoint_rel_ms cp1( p1 );
        CAPTURE( cp0, cp1 );

        CHECK( ( p0 < p1 ) == ( cp0 < cp1 ) );
        CHECK( ( p0 == p1 ) == ( cp0 == cp1 ) );
        CHECK( cp0 == cp0 );
        CHECK( !( cp0 != cp0 ) );
    }
}

TEST_CASE( "coordinate_hash", "[point][coords]" )
{
    SECTION( "point_hash" ) {
        point p = GENERATE( take( num_trials, random_points() ) );
        point_abs_ms cp( p );
        CHECK( std::hash<point_abs_ms>()( cp ) == std::hash<point>()( p ) );
    }

    SECTION( "tripoint_hash" ) {
        tripoint p = GENERATE( take( num_trials, random_tripoints() ) );
        tripoint_abs_ms cp( p );
        CHECK( std::hash<tripoint_abs_ms>()( cp ) == std::hash<tripoint>()( p ) );
    }
}


TEST_CASE( "combine_is_opposite_of_remain", "[point][coords]" )
{
    SECTION( "point_point" ) {
        point p = GENERATE( take( num_trials, random_points() ) );
        CAPTURE( p );
        point_abs_sm orig( p );
        point_abs_om quotient;
        point_om_sm remainder;
        std::tie( quotient, remainder ) = project_remain<coords::om>( orig );
        point_abs_sm recombined = project_combine( quotient, remainder );
        CHECK( recombined == orig );
    }
    SECTION( "tripoint_point" ) {
        tripoint p = GENERATE( take( num_trials, random_tripoints() ) );
        CAPTURE( p );
        tripoint_abs_sm orig( p );
        tripoint_abs_om quotient;
        point_om_sm remainder;
        std::tie( quotient, remainder ) = project_remain<coords::om>( orig );
        tripoint_abs_sm recombined = project_combine( quotient, remainder );
        CHECK( recombined == orig );
    }
    SECTION( "point_tripoint" ) {
        tripoint p = GENERATE( take( num_trials, random_tripoints() ) );
        CAPTURE( p );
        tripoint_abs_sm orig( p );
        point_abs_om quotient;
        tripoint_om_sm remainder;
        std::tie( quotient, remainder ) = project_remain<coords::om>( orig );
        tripoint_abs_sm recombined = project_combine( quotient, remainder );
        CHECK( recombined == orig );
    }
}

TEST_CASE( "coord_point_distances", "[point][coords]" )
{
    point_abs_omt p0;
    point_abs_omt p1( 10, 10 );
    tripoint_abs_omt t0;
    tripoint_abs_omt t1( 10, 10, 10 );

    SECTION( "square" ) {
        CHECK( square_dist( p0, p1 ) == 10 );
        CHECK( square_dist( t0, t1 ) == 10 );
    }

    SECTION( "trig" ) {
        CHECK( std::abs( trig_dist( p0, p1 ) - 14.1421 ) < 0.001 );
        CHECK( std::abs( trig_dist( t0, t1 ) - 17.3205 ) < 0.001 );
    }

    SECTION( "manhattan" ) {
        CHECK( manhattan_dist( p0, p1 ) == 20 );
    }
}

TEST_CASE( "coord_point_midpoint", "[point][coords]" )
{
    point_abs_omt p0( 2, 2 );
    point_abs_omt p1( 8, 17 );
    tripoint_abs_omt t0( 2, 2, 2 );
    tripoint_abs_omt t1( 8, 17, 5 );

    CHECK( midpoint( p0, p1 ) == point_abs_omt( 5, 9 ) );
    CHECK( midpoint( t0, t1 ) == tripoint_abs_omt( 5, 9, 3 ) );
}
