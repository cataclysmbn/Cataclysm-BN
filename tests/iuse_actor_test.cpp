#include "catch/catch.hpp"

#include <climits>
#include <memory>

#include "avatar.h"
#include "cata_utility.h"
#include "coordinates.h"
#include "game.h"
#include "item.h"
#include "monster.h"
#include "overmap_special.h"
#include "mtype.h"
#include "player.h"
#include "state_helpers.h"
#include "string_id.h"
#include "type_id.h"

static monster *find_adjacent_monster( const tripoint_bub_ms &pos )
{
    tripoint_bub_ms target = pos;
    for( target.x() = pos.x() - 1; target.x() <= pos.x() + 1; target.x()++ ) {
        for( target.y() = pos.y() - 1; target.y() <= pos.y() + 1; target.y()++ ) {
            if( target == pos ) {
                continue;
            }
            if( monster *const candidate = g->critter_at<monster>( target ) ) {
                return candidate;
            }
        }
    }
    return nullptr;
}

TEST_CASE( "debug_backroom_key_opens_backroom_pocket", "[iuse_actor][pocket_dimension][backroom]" )
{
    clear_all_state();
    const auto cleanup = on_out_of_scope( []() {
        clear_all_state();
    } );

    static const auto debug_backroom_key = itype_id( "debug_backroom_key" );
    static const auto debug_backroom_pocket = overmap_special_id( "debug_backroom_pocket" );
    static const auto pocket_dimension = world_type_id( "pocket_dimension" );

    REQUIRE( debug_backroom_key.is_valid() );
    REQUIRE( debug_backroom_pocket.is_valid() );

    player &dummy = get_avatar();
    g->place_player( tripoint_bub_ms( 60, 60, 0 ) );
    const auto origin_dim = g->get_current_dimension_id();
    const auto origin = dummy.abs_pos();

    auto det = item::spawn( debug_backroom_key, calendar::start_of_cataclysm,
                            item::default_charges_tag{} );
    item &key = *det;
    dummy.i_add( std::move( det ) );

    dummy.invoke_item( &key );

    const auto *pocket_info = g->get_current_dimension_info();
    REQUIRE( pocket_info != nullptr );
    CHECK( g->get_current_dimension_id() != origin_dim );
    CHECK( pocket_info->world_type == pocket_dimension );
    REQUIRE( pocket_info->pocket_info.has_value() );
    CHECK( pocket_info->pocket_info->bounds.contains( dummy.abs_pos() ) );

    dummy.invoke_item( &key );

    CHECK( g->get_current_dimension_id() == origin_dim );
    CHECK( dummy.abs_pos() == origin );
}

TEST_CASE( "manhack", "[iuse_actor][manhack]" )
{
    clear_all_state();
    player &dummy = get_avatar();

    g->clear_zombies();
    detached_ptr<item> det = item::spawn( "bot_manhack", calendar::start_of_cataclysm,
                                          item::default_charges_tag{} );
    item &test_item = *det;
    dummy.i_add( std::move( det ) );

    int test_item_pos = dummy.inv_position_by_item( &test_item );
    REQUIRE( test_item_pos != INT_MIN );

    monster *new_manhack = find_adjacent_monster( dummy.bub_pos() );
    REQUIRE( new_manhack == nullptr );

    dummy.invoke_item( &test_item );

    test_item_pos = dummy.inv_position_by_item( &test_item );
    REQUIRE( test_item_pos == INT_MIN );

    new_manhack = find_adjacent_monster( dummy.bub_pos() );
    REQUIRE( new_manhack != nullptr );
    REQUIRE( new_manhack->type->id == mtype_id( "mon_manhack" ) );
    g->clear_zombies();
}
