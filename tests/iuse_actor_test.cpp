#include "catch/catch.hpp"

#include <climits>
#include <memory>

#include "avatar.h"
#include "cata_utility.h"
#include "coordinates.h"
#include "game.h"
#include "item.h"
#include "monster.h"
#include "mtype.h"
#include "player.h"
#include "scenario.h"
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

TEST_CASE( "backroom_scenario_starts_in_backroom_dimension", "[scenario][dimension][backroom]" )
{
    static const auto backroom_dimension = world_type_id( "backroom" );
    static const auto backroom_dweller = string_id<scenario>( "backroom_dweller" );

    REQUIRE( backroom_dimension.is_valid() );
    REQUIRE( backroom_dweller.is_valid() );
    CHECK( backroom_dweller.obj().start_dimension() == backroom_dimension );
}

TEST_CASE( "debug_backroom_key_opens_backroom_dimension", "[iuse_actor][dimension][backroom]" )
{
    clear_all_state();
    const auto cleanup = on_out_of_scope( []() {
        clear_all_state();
    } );

    static const auto debug_backroom_key = itype_id( "debug_backroom_key" );
    static const auto backroom_dimension = world_type_id( "backroom" );

    REQUIRE( debug_backroom_key.is_valid() );
    REQUIRE( backroom_dimension.is_valid() );

    player &dummy = get_avatar();
    g->place_player( tripoint_bub_ms( 60, 60, 0 ) );
    const auto origin_dim = g->get_current_dimension_id();
    const auto origin = dummy.abs_pos();

    auto det = item::spawn( debug_backroom_key, calendar::start_of_cataclysm,
                            item::default_charges_tag{} );
    item &key = *det;
    dummy.i_add( std::move( det ) );

    dummy.invoke_item( &key );

    const auto *backroom_info = g->get_current_dimension_info();
    REQUIRE( backroom_info != nullptr );
    CHECK( g->get_current_dimension_id() == "backroom" );
    CHECK( backroom_info->world_type == backroom_dimension );
    CHECK_FALSE( backroom_info->pocket_info.has_value() );

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
