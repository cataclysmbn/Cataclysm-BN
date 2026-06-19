#include "catch/catch.hpp"

#include <algorithm>
#include <array>
#include <string>
#include <vector>

#include "calendar.h"
#include "catalua_impl.h"
#include "coordinates.h"
#include "init.h"
#include "mapbuffer_registry.h"
#include "mapgen.h"
#include "mapgen_constructor.h"
#include "mapgendata.h"
#include "overmap.h"
#include "overmap_special.h"
#include "overmapbuffer.h"
#include "overmapbuffer_registry.h"
#include "state_helpers.h"
#include "type_id.h"

namespace
{

struct linked_lab_positions {
    tripoint_abs_omt upper;
    tripoint_abs_omt lower;
};

auto insert_lab_stairs( mapgendata &dat, const bool from_above ) -> void
{
    auto &lua = DynamicDataLoader::get_instance().lua->lua;
    auto require = sol::protected_function( lua["require"] );
    auto require_result = require( "lua/mapgen/lab" );
    check_func_result( require_result );
    auto insert_stairs = sol::protected_function( lua.globals()["insert_stairs"] );
    auto insert_result = insert_stairs( &dat, &dat.m, ter_id( "t_stairs_up" ),
                                        ter_id( "t_stairs_down" ), from_above );
    check_func_result( insert_result );
}

auto lab_points_with_terrain_flag( const tripoint_abs_omt &pos, const bool from_above,
                                   const std::string &flag ) -> std::vector<point_omt_ms>
{
    auto &buffer = MAPBUFFER_REGISTRY.get( mapbuffer_registry::primary_dimension_id() );
    auto tm = mapgen_constructor( buffer );
    tm.reset_scratch_omt( pos, ter_id( "t_thconc_floor" ), furn_id( "f_null" ), trap_id( "tr_null" ) );
    auto &omb = get_overmapbuffer( buffer.get_dimension_id() );
    auto dat = mapgendata( pos, tm, 0.0f, calendar::turn, nullptr, omb );
    insert_lab_stairs( dat, from_above );

    auto result = std::vector<point_omt_ms> {};
    for( const auto p : tm.points_on_zlevel() ) {
        if( tm.has_flag_ter( flag, p ) ) {
            result.push_back( p );
        }
    }
    return result;
}

auto has_sewer_neighbor( overmapbuffer &omb, const tripoint_abs_omt &pos ) -> bool
{
    static constexpr auto directions = std::array<tripoint, 4> {
        tripoint_north, tripoint_east, tripoint_south, tripoint_west
    };
    return std::ranges::any_of( directions, [&]( const tripoint & direction ) {
        return is_ot_match( "sewer", omb.ter( pos + direction ), ot_match_type::type );
    } );
}

auto first_lab_stair_pair( overmapbuffer &omb,
                           const std::vector<tripoint_abs_omt> &placed ) -> linked_lab_positions
{
    for( const auto &pos : placed ) {
        if( !is_ot_match( "stairs", omb.ter( pos ), ot_match_type::contains ) ) {
            continue;
        }
        const auto lower = pos + tripoint_below;
        if( std::ranges::find( placed, lower ) == placed.end() ) {
            continue;
        }
        if( is_ot_match( "lab", omb.ter( lower ), ot_match_type::contains ) &&
            !has_sewer_neighbor( omb, pos ) && !has_sewer_neighbor( omb, lower ) ) {
            return { .upper = pos, .lower = lower };
        }
    }
    FAIL( "placed lab special did not contain a vertical stair pair" );
    return {};
}

auto prepare_lab_stair_pair() -> linked_lab_positions
{
    clear_all_state();
    auto &omb = get_overmapbuffer( mapbuffer_registry::primary_dimension_id() );
    static auto next_origin = int{ 500000 };
    const auto origin = tripoint_abs_omt( next_origin, next_origin, -2 );
    next_origin += 20;
    const auto placed = omb.place_special( *overmap_special_id( "lab_basement" ), origin,
                                           om_direction::type::north, false, true );
    REQUIRE( placed.has_value() );
    return first_lab_stair_pair( omb, *placed );
}

auto check_lab_stair_pair_links( const bool generate_upper_first ) -> void
{
    const auto positions = prepare_lab_stair_pair();
    auto &omb = get_overmapbuffer( mapbuffer_registry::primary_dimension_id() );
    INFO( "upper " << positions.upper.to_string() << " " << omb.ter( positions.upper ).id().str() );
    INFO( "lower " << positions.lower.to_string() << " " << omb.ter( positions.lower ).id().str() );
    auto upper_down = std::vector<point_omt_ms> {};
    auto lower_up = std::vector<point_omt_ms> {};
    if( generate_upper_first ) {
        upper_down = lab_points_with_terrain_flag( positions.upper, false, "GOES_DOWN" );
        lower_up = lab_points_with_terrain_flag( positions.lower, true, "GOES_UP" );
    } else {
        lower_up = lab_points_with_terrain_flag( positions.lower, true, "GOES_UP" );
        upper_down = lab_points_with_terrain_flag( positions.upper, false, "GOES_DOWN" );
    }

    INFO( "upper down count " << upper_down.size() );
    INFO( "lower up count " << lower_up.size() );
    REQUIRE( upper_down.size() == 1 );
    REQUIRE( lower_up.size() == 1 );
    CHECK( upper_down.front() == lower_up.front() );
}

} // namespace

TEST_CASE( "lua_lab_mapgen_links_stairs_from_vertical_join_anchor", "[mapgen][lua][lab]" )
{
    SECTION( "upper generated first" ) {
        check_lab_stair_pair_links( true );
    }

    SECTION( "lower generated first" ) {
        check_lab_stair_pair_links( false );
    }
}

TEST_CASE( "connects_to", "[mapgen][connects]" )
{
    // connects_to returns true if a given oter connects to a given cardinal
    // compass direction, identified by an integer, clockwise from 0:North

    int north = 0;
    int east = 1;
    int south = 2;
    int west = 3;

    // oter suffixes must be one of the following (order matters):
    // ne, ns, es, nes, wn, ew, new, sw, nsw, esw, nesw

    SECTION( "two-way connections" ) {
        // North/South
        CHECK( connects_to( oter_id( "sewer_ns" ), north ) );
        CHECK_FALSE( connects_to( oter_id( "sewer_ns" ), east ) );
        CHECK( connects_to( oter_id( "sewer_ns" ), south ) );
        CHECK_FALSE( connects_to( oter_id( "sewer_ns" ), west ) );

        // East/West
        CHECK_FALSE( connects_to( oter_id( "sewer_ew" ), north ) );
        CHECK( connects_to( oter_id( "sewer_ew" ), east ) );
        CHECK_FALSE( connects_to( oter_id( "sewer_ew" ), south ) );
        CHECK( connects_to( oter_id( "sewer_ew" ), west ) );

        // North/East
        CHECK( connects_to( oter_id( "sewer_ne" ), north ) );
        CHECK( connects_to( oter_id( "sewer_ne" ), east ) );
        CHECK_FALSE( connects_to( oter_id( "sewer_ne" ), south ) );
        CHECK_FALSE( connects_to( oter_id( "sewer_ne" ), west ) );

        // East/South
        CHECK_FALSE( connects_to( oter_id( "sewer_es" ), north ) );
        CHECK( connects_to( oter_id( "sewer_es" ), east ) );
        CHECK( connects_to( oter_id( "sewer_es" ), south ) );
        CHECK_FALSE( connects_to( oter_id( "sewer_es" ), west ) );

        // South/West
        CHECK_FALSE( connects_to( oter_id( "sewer_sw" ), north ) );
        CHECK_FALSE( connects_to( oter_id( "sewer_sw" ), east ) );
        CHECK( connects_to( oter_id( "sewer_sw" ), south ) );
        CHECK( connects_to( oter_id( "sewer_sw" ), west ) );

        // West/North
        CHECK( connects_to( oter_id( "sewer_wn" ), north ) );
        CHECK_FALSE( connects_to( oter_id( "sewer_wn" ), east ) );
        CHECK_FALSE( connects_to( oter_id( "sewer_wn" ), south ) );
        CHECK( connects_to( oter_id( "sewer_wn" ), west ) );
    }

    SECTION( "three-way connections" ) {
        // North/East/South
        CHECK( connects_to( oter_id( "sewer_nes" ), north ) );
        CHECK( connects_to( oter_id( "sewer_nes" ), east ) );
        CHECK( connects_to( oter_id( "sewer_nes" ), south ) );
        CHECK_FALSE( connects_to( oter_id( "sewer_nes" ), west ) );

        // East/South/West
        CHECK_FALSE( connects_to( oter_id( "sewer_esw" ), north ) );
        CHECK( connects_to( oter_id( "sewer_esw" ), east ) );
        CHECK( connects_to( oter_id( "sewer_esw" ), south ) );
        CHECK( connects_to( oter_id( "sewer_esw" ), west ) );

        // South/West/North
        CHECK( connects_to( oter_id( "sewer_nsw" ), north ) );
        CHECK_FALSE( connects_to( oter_id( "sewer_nsw" ), east ) );
        CHECK( connects_to( oter_id( "sewer_nsw" ), south ) );
        CHECK( connects_to( oter_id( "sewer_nsw" ), west ) );

        // West/North/East
        CHECK( connects_to( oter_id( "sewer_new" ), north ) );
        CHECK( connects_to( oter_id( "sewer_new" ), east ) );
        CHECK_FALSE( connects_to( oter_id( "sewer_new" ), south ) );
        CHECK( connects_to( oter_id( "sewer_new" ), west ) );
    }

    SECTION( "four-way connections" ) {
        // North/East/South/West
        CHECK( connects_to( oter_id( "sewer_nesw" ), north ) );
        CHECK( connects_to( oter_id( "sewer_nesw" ), east ) );
        CHECK( connects_to( oter_id( "sewer_nesw" ), south ) );
        CHECK( connects_to( oter_id( "sewer_nesw" ), west ) );
    }
}
