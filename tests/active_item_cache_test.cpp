#include "catch/catch.hpp"

#include <memory>
#include <set>

#include "calendar.h"
#include "game.h"
#include "game_constants.h"
#include "item.h"
#include "map.h"
#include "point.h"
#include "state_helpers.h"

TEST_CASE( "place_active_item_at_various_coordinates", "[item]" )
{
    clear_all_state();
    for( int z = -OVERMAP_DEPTH; z < OVERMAP_HEIGHT; ++z ) {
        for( int x = 0; x < g_mapsize_x; ++x ) {
            for( int y = 0; y < g_mapsize_y; ++y ) {
                g->m.i_clear( { x, y, z } );
            }
        }
    }
    REQUIRE( g->m.get_submaps_with_active_items().empty() );
    // An arbitrary active item.
    item &active = *item::spawn_temporary( "firecracker_act", calendar::start_of_cataclysm,
                                           item::default_charges_tag() );
    active.activate();

    // For each space in a wide area place the item and check if the cache has been updated.
    int z = 0;
    for( int x = 0; x < g_mapsize_x; ++x ) {
        for( int y = 0; y < g_mapsize_y; ++y ) {
            REQUIRE( g->m.i_at( { x, y, z } ).empty() );
            CAPTURE( x, y, z );
            tripoint abs_loc = g->m.get_abs_sub() + tripoint( x / SEEX, y / SEEY, z );
            CAPTURE( abs_loc.x, abs_loc.y, abs_loc.z );
            REQUIRE( g->m.get_submaps_with_active_items().empty() );
            REQUIRE( g->m.get_submaps_with_active_items().find( abs_loc ) ==
                     g->m.get_submaps_with_active_items().end() );
            detached_ptr<item> n = item::spawn( active );
            item &item_ref = *n;
            g->m.add_item( { x, y, z }, std::move( n ) );
            REQUIRE( item_ref.is_active() );
            REQUIRE_FALSE( g->m.get_submaps_with_active_items().empty() );
            REQUIRE( g->m.get_submaps_with_active_items().find( abs_loc ) !=
                     g->m.get_submaps_with_active_items().end() );
            REQUIRE_FALSE( g->m.i_at( { x, y, z } ).empty() );
            g->m.i_clear( { x, y, z } );
        }
    }
}
