#include "catch/catch.hpp"

#include <utility>

#include "calendar.h"
#include "coordinates.h"
#include "game.h"
#include "game_constants.h"
#include "item.h"
#include "map.h"
#include "state_helpers.h"

TEST_CASE( "place_active_item_at_various_coordinates", "[item]" )
{
    clear_all_state();
    for( auto z = -OVERMAP_DEPTH; z <= OVERMAP_HEIGHT; ++z ) {
        for( auto x = 0; x < g_mapsize_x; ++x ) {
            for( auto y = 0; y < g_mapsize_y; ++y ) {
                g->m.i_clear( tripoint_bub_ms{ x, y, z } );
            }
        }
    }
    REQUIRE( g->m.get_submaps_with_active_items().empty() );
    // An arbitrary active item.
    auto &active = *item::spawn_temporary( "firecracker_act", calendar::start_of_cataclysm,
                                           item::default_charges_tag() );
    active.activate();

    // For each space in a wide area place the item and check if the cache has been updated.
    const auto z = 0;
    for( auto x = 0; x < g_mapsize_x; ++x ) {
        for( auto y = 0; y < g_mapsize_y; ++y ) {
            REQUIRE( g->m.i_at( tripoint_bub_ms{ x, y, z } ).empty() );
            CAPTURE( x, y, z );
            const auto abs_loc = g->m.get_abs_sub() + tripoint_rel_sm( x / SEEX, y / SEEY, z );
            CAPTURE( abs_loc.x(), abs_loc.y(), abs_loc.z() );
            REQUIRE( g->m.get_submaps_with_active_items().empty() );
            REQUIRE( g->m.get_submaps_with_active_items().find( abs_loc ) ==
                     g->m.get_submaps_with_active_items().end() );
            auto n = item::spawn( active );
            auto &item_ref = *n;
            g->m.add_item( tripoint_bub_ms{ x, y, z }, std::move( n ) );
            REQUIRE( item_ref.is_active() );
            REQUIRE_FALSE( g->m.get_submaps_with_active_items().empty() );
            REQUIRE( g->m.get_submaps_with_active_items().find( abs_loc ) !=
                     g->m.get_submaps_with_active_items().end() );
            REQUIRE_FALSE( g->m.i_at( tripoint_bub_ms{ x, y, z } ).empty() );
            g->m.i_clear( tripoint_bub_ms{ x, y, z } );
        }
    }
}
