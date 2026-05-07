#include "catch/catch.hpp"

#include <cstddef>
#include <list>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>

#include "activity_actor.h"
#include "activity_actor_definitions.h"
#include "avatar.h"
#include "coordinates.h"
#include "game_inventory.h"
#include "inventory.h"
#include "item.h"
#include "map.h"
#include "map_helpers.h"
#include "map_selector.h"
#include "options_helpers.h"
#include "player.h"
#include "player_activity.h"
#include "state_helpers.h"
#include "type_id.h"
#include "visitable.h"

const trait_id trait_debug_storage( "DEBUG_STORAGE" );

enum inventory_location {
    GROUND,
    INVENTORY,
    WORN,
    WIELDED_OR_WORN,
    INV_LOCATION_NUM,
};

enum invlet_state {
    UNEXPECTED = -1,
    NONE = 0,
    CACHED,
    ASSIGNED,
    INVLET_STATE_NUM,
};

enum test_action {
    REMOVE_1ST_REMOVE_2ND_ADD_1ST_ADD_2ND,
    REMOVE_1ST_REMOVE_2ND_ADD_2ND_ADD_1ST,
    REMOVE_1ST_ADD_1ST,
    TEST_ACTION_NUM,
};

static std::string location_desc( const inventory_location loc )
{
    switch( loc ) {
        case GROUND:
            return "the ground";
        case INVENTORY:
            return "inventory";
        case WORN:
            return "worn items";
        case WIELDED_OR_WORN:
            return "wielded or worn items";
        default:
            break;
    }
    return "unknown location";
}

static std::string move_action_desc( const int pos, const inventory_location from,
                                     const inventory_location to )
{
    std::stringstream ss;
    ss << "move ";
    switch( pos ) {
        case 0:
            ss << "1st";
            break;
        case 1:
            ss << "2nd";
            break;
        default:
            return "unimplemented";
    }
    ss << " item ";
    switch( from ) {
        case GROUND:
            ss << "on the ground";
            break;
        case INVENTORY:
            ss << "in inventory";
            break;
        case WORN:
            ss << "worn";
            break;
        case WIELDED_OR_WORN:
            ss << "wielded or worn";
            break;
        default:
            return "unimplemented";
    }
    ss << " to ";
    switch( to ) {
        case GROUND:
            ss << "the ground";
            break;
        case INVENTORY:
            ss << "inventory";
            break;
        case WORN:
            ss << "worn items";
            break;
        case WIELDED_OR_WORN:
            ss << "wielded or worn items";
            break;
        default:
            return "unimplemented";
    }
    return ss.str();
}

static std::string invlet_state_desc( const invlet_state invstate )
{
    switch( invstate ) {
        case NONE:
            return "none";
        case CACHED:
            return "cached";
        case ASSIGNED:
            return "assigned";
        default:
            break;
    }
    return "unexpected";
}

static std::string test_action_desc(
    const test_action action, const inventory_location from, const inventory_location to,
    const invlet_state first_invlet_state, const invlet_state second_invlet_state,
    const invlet_state expected_first_invlet_state,
    const invlet_state expected_second_invlet_state,
    const invlet_state final_first_invlet_state, const invlet_state final_second_invlet_state )
{
    std::stringstream ss;
    ss << "1. add 1st item to " << location_desc( to ) << '\n';
    ss << "2. add 2nd item to " << location_desc( to ) << '\n';
    ss << "3. set 1st item's invlet to " << invlet_state_desc( first_invlet_state ) << '\n';
    ss << "4. " << move_action_desc( 0, to, from ) << '\n';
    ss << "5. set 2nd item's invlet to " << invlet_state_desc( second_invlet_state ) << '\n';
    switch( action ) {
        case REMOVE_1ST_REMOVE_2ND_ADD_1ST_ADD_2ND:
            ss << "6. " << move_action_desc( 1, to, from ) << '\n';
            ss << "7. " << move_action_desc( 0, from, to ) << '\n';
            ss << "8. " << move_action_desc( 1, from, to ) << '\n';
            break;
        case REMOVE_1ST_REMOVE_2ND_ADD_2ND_ADD_1ST:
            ss << "6. " << move_action_desc( 1, to, from ) << '\n';
            ss << "7. " << move_action_desc( 1, from, to ) << '\n';
            ss << "8. " << move_action_desc( 0, from, to ) << '\n';
            break;
        case REMOVE_1ST_ADD_1ST:
            ss << "6. " << move_action_desc( 0, from, to ) << '\n';
            break;
        default:
            return "unimplemented";
    }
    ss << "expect 1st item to have " << invlet_state_desc( expected_first_invlet_state ) << " invlet" <<
       '\n';
    ss << "1st item actually has " << invlet_state_desc( final_first_invlet_state ) << " invlet" <<
       '\n';
    ss << "expect 2nd item to have " << invlet_state_desc( expected_second_invlet_state ) << " invlet"
       << '\n';
    ss << "2nd item actually has " << invlet_state_desc( final_second_invlet_state ) << " invlet" <<
       '\n';

    return ss.str();
}

static void assign_invlet( Character &p, item &it, const char invlet, const invlet_state invstate )
{
    using game_menus::inv::reassign_letter;
    reassign_letter( p, it, '\0' );
    switch( invstate ) {
        case NONE:
            break;
        case CACHED:
            // assigning it twice makes it a cached but non-player-assigned invlet
            reassign_letter( p, it, invlet );
            reassign_letter( p, it, invlet );
            break;
        case ASSIGNED:
            reassign_letter( p, it, invlet );
            break;
        default:
            FAIL( "unimplemented" );
    }
}

static invlet_state check_invlet( player &p, item &it, const char invlet )
{
    if( it.invlet == '\0' ) {
        return NONE;
    } else if( it.invlet == invlet ) {
        if( p.inv_assigned_invlet().find( invlet ) != p.inv_assigned_invlet().end() &&
            p.inv_assigned_invlet()[invlet] == it.typeId() ) {
            return ASSIGNED;
        } else {
            return CACHED;
        }
    }
    return UNEXPECTED;
}

static void drop_at_feet( player &p, item &it )
{
    size_t size_before = get_map().i_at( p.bub_pos() ).size();

    p.moves = 100;
    p.drop( it, p.bub_pos() );
    p.activity->do_turn( p );

    REQUIRE( get_map().i_at( p.bub_pos() ).size() == size_before + 1 );
}

static void pick_up_from_feet( player &p, item &it )
{
    map_stack items = get_map().i_at( p.bub_pos() );
    size_t size_before = items.size();

    p.moves = 100;
    p.assign_activity( std::make_unique<player_activity>(
    std::make_unique<pickup_activity_actor>( std::vector<pickup::pick_drop_selection> { { it, 0, {} } },
    p.bub_pos() ) ) );
    p.activity->do_turn( p );

    REQUIRE( items.size() == size_before - 1 );
}

static void wear_from_feet( player &p, item &it )
{
    map_stack items = get_map().i_at( p.bub_pos() );
    size_t size_before = items.size();

    p.wear_item( it.detach(), false );

    REQUIRE( items.size() == size_before - 1 );
}

static void wield_from_feet( player &p, item &it )
{
    map_stack items = get_map().i_at( p.bub_pos() );
    size_t size_before = items.size();


    p.wield( it.detach() );

    REQUIRE( items.size() == size_before - 1 );
}

static void add_item( player &p, detached_ptr<item> &&it, const inventory_location loc )
{
    switch( loc ) {
        case GROUND:
            get_map().add_item( p.bub_pos(), std::move( it ) );
            break;
        case INVENTORY:
            p.i_add( std::move( it ) );
            break;
        case WORN:
            p.wear_item( std::move( it ) );
            break;
        case WIELDED_OR_WORN:
            if( !p.is_armed() ) {
                p.wield( std::move( it ) );
            } else {
                // since we can only wield one item, wear the item instead
                p.wear_item( std::move( it ) );
            }
            break;
        default:
            FAIL( "unimplemented" );
            break;
    }
}

static void move_item( player &p, item &it, const inventory_location from,
                       const inventory_location to )
{
    switch( from ) {
        case GROUND:
            switch( to ) {
                case GROUND:
                default:
                    FAIL( "unimplemented" );
                    break;
                case INVENTORY:
                    pick_up_from_feet( p, it );
                    break;
                case WORN:
                    wear_from_feet( p, it );
                    break;
                case WIELDED_OR_WORN:
                    if( p.primary_weapon().is_null() ) {
                        wield_from_feet( p, it );
                    } else {
                        // since we can only wield one item, wear the item instead
                        wear_from_feet( p, it );
                    }
                    break;
            }
            break;
        case INVENTORY:
            switch( to ) {
                case GROUND:
                    drop_at_feet( p, it );
                    break;
                case INVENTORY:
                default:
                    FAIL( "unimplemented" );
                    break;
                case WORN:
                    p.wear_possessed( it, false );
                    break;
                case WIELDED_OR_WORN:
                    if( p.primary_weapon().is_null() ) {
                        p.wield( it );
                    } else {
                        // since we can only wield one item, wear the item instead
                        p.wear_possessed( it, false );
                    }
                    break;
            }
            break;
        case WORN:
            switch( to ) {
                case GROUND:
                    drop_at_feet( p, it );
                    break;
                case INVENTORY:
                    p.takeoff( it );
                    break;
                case WORN:
                case WIELDED_OR_WORN:
                default:
                    FAIL( "unimplemented" );
                    break;
            }
            break;
        case WIELDED_OR_WORN:
            switch( to ) {
                case GROUND:
                    drop_at_feet( p, it );
                    if( !p.is_armed() && !p.worn.empty() ) {
                        // wield the first worn item
                        p.wield( p.worn.front()->detach() );
                    }
                    break;
                case INVENTORY:
                    if( p.is_wielding( it ) ) {
                        p.i_add( it.detach( ) );
                    } else {
                        p.takeoff( it );
                    }
                    if( !p.is_armed() && !p.worn.empty() ) {
                        // wield the first worn item
                        p.wield( p.worn.front()->detach() );
                    }
                    break;
                case WORN:
                case WIELDED_OR_WORN:
                default:
                    FAIL( "unimplemented" );
                    break;
            }
            break;
        default:
            FAIL( "unimplemented" );
            break;
    }
}

static void invlet_test( player &dummy, const inventory_location from, const inventory_location to )
{
    // invlet to assign
    constexpr char invlet = '|';

    // iterate through all permutations of test actions
    for( int id = 0; id < INVLET_STATE_NUM * INVLET_STATE_NUM * TEST_ACTION_NUM; ++id ) {
        // how to assign invlet to the first item
        const invlet_state first_invlet_state = invlet_state( id % INVLET_STATE_NUM );
        // how to assign invlet to the second item
        const invlet_state second_invlet_state = invlet_state( id / INVLET_STATE_NUM % INVLET_STATE_NUM );
        // the test steps
        const test_action action = test_action( id / INVLET_STATE_NUM / INVLET_STATE_NUM %
                                                TEST_ACTION_NUM );

        // the final expected invlet state of the two items
        invlet_state expected_first_invlet_state = second_invlet_state == NONE ? first_invlet_state : NONE;
        invlet_state expected_second_invlet_state = second_invlet_state;

        // remove all items
        dummy.inv_clear();
        dummy.worn.clear();
        dummy.remove_primary_weapon();
        get_map().i_clear( dummy.bub_pos() );


        // some two items that can be wielded, worn, and picked up
        detached_ptr<item> tshirt_d = item::spawn( "tshirt" );
        item &tshirt = *tshirt_d;
        detached_ptr<item> jeans_d = item::spawn( "jeans" );
        item &jeans = *jeans_d;

        // add the items to the starting position
        add_item( dummy, std::move( tshirt_d ), to );
        add_item( dummy, std::move( jeans_d ), to );

        // assign invlet to the first item
        assign_invlet( dummy, tshirt, invlet, first_invlet_state );

        // remove the first item
        move_item( dummy, tshirt, to, from );

        // assign invlet to the second item
        assign_invlet( dummy, jeans, invlet, second_invlet_state );

        item *final_first = nullptr;
        item *final_second = nullptr;
        switch( action ) {
            case REMOVE_1ST_REMOVE_2ND_ADD_1ST_ADD_2ND:
                move_item( dummy, jeans, to, from );
                move_item( dummy, tshirt, from, to );
                move_item( dummy, jeans, from, to );
                final_first = &tshirt;
                final_second = &jeans;
                break;
            case REMOVE_1ST_REMOVE_2ND_ADD_2ND_ADD_1ST:
                move_item( dummy, jeans, to, from );
                move_item( dummy, jeans, from, to );
                move_item( dummy, tshirt, from, to );
                final_first = &tshirt;
                final_second = &jeans;
                break;
            case REMOVE_1ST_ADD_1ST:
                move_item( dummy, tshirt, from, to );
                final_first = &tshirt;
                final_second = &jeans;
                break;
            default:
                FAIL( "unimplemented" );
                break;
        }

        invlet_state final_first_invlet_state = check_invlet( dummy, *final_first, invlet ),
                     final_second_invlet_state = check_invlet( dummy, *final_second, invlet );

        INFO( test_action_desc( action, from, to, first_invlet_state, second_invlet_state,
                                expected_first_invlet_state, expected_second_invlet_state, final_first_invlet_state,
                                final_second_invlet_state ) );
        REQUIRE( final_first->typeId() == tshirt.typeId() );
        REQUIRE( final_second->typeId() == jeans.typeId() );
        CHECK( final_first_invlet_state == expected_first_invlet_state );
        CHECK( final_second_invlet_state == expected_second_invlet_state );

        // clear invlets
        assign_invlet( dummy, *final_first, invlet, NONE );
        assign_invlet( dummy, *final_second, invlet, NONE );
    }
}

static void stack_invlet_test( player &dummy, inventory_location from, inventory_location to )
{
    // invlet to assign
    constexpr char invlet = '|';

    // duplication will most likely only happen if the stack is in the inventory
    // and is subsequently wielded or worn
    if( from != INVENTORY || ( to != WORN && to != WIELDED_OR_WORN ) ) {
        FAIL( "unimplemented" );
    }

    // remove all items
    dummy.inv_clear();
    dummy.worn.clear();
    dummy.remove_primary_weapon();
    get_map().i_clear( dummy.bub_pos() );

    // some stackable item that can be wielded and worn
    detached_ptr<item> sd1 = item::spawn( "tshirt" );
    item &tshirt1 = *sd1;
    detached_ptr<item> sd2 = item::spawn( "tshirt" );
    item &tshirt2 = *sd2;

    // add two such items to the starting position
    add_item( dummy, std::move( sd1 ), from );
    add_item( dummy, std::move( sd2 ), from );

    // assign the stack with invlet
    assign_invlet( dummy, tshirt1, invlet, CACHED );

    // wield or wear the first item
    move_item( dummy, tshirt1, from, to );

    std::stringstream ss;
    ss << "1. add a stack of two same items to " << location_desc( from ) << '\n';
    ss << "2. assign the stack with an invlet" << '\n';
    ss << "3. " << move_action_desc( 0, from, to ) << '\n';
    ss << "expect the two items to have different invlets" << '\n';
    ss << "actually the two items have " <<
       ( tshirt1.invlet != tshirt2.invlet ? "different" : "the same" ) <<
       " invlets" << '\n';
    INFO( ss.str() );
    // the wielded/worn item should have different invlet from the remaining item
    CHECK( tshirt1.invlet != tshirt2.invlet );

    // clear invlets
    assign_invlet( dummy, tshirt1, invlet, NONE );
    assign_invlet( dummy, tshirt2, invlet, NONE );
}

static void swap_invlet_test( player &dummy, inventory_location loc )
{
    // invlet to assign
    constexpr char invlet_1 = '{';
    constexpr char invlet_2 = '}';

    // cannot swap invlets of items on the ground
    REQUIRE( loc != GROUND );

    // remove all items
    dummy.inv_clear();
    dummy.worn.clear();
    dummy.remove_primary_weapon();
    get_map().i_clear( dummy.bub_pos() );

    // two items of the same type that do not stack
    detached_ptr<item> sd1 = item::spawn( "tshirt" );
    item &tshirt1 = *sd1;
    detached_ptr<item> sd2 = item::spawn( "tshirt" );
    item &tshirt2 = *sd2;
    tshirt2.mod_damage( -1 );

    // add the items
    add_item( dummy, std::move( sd1 ), loc );
    add_item( dummy, std::move( sd2 ), loc );

    // assign the items with invlets
    assign_invlet( dummy, tshirt1, invlet_1, CACHED );
    assign_invlet( dummy, tshirt2, invlet_2, CACHED );

    // swap the invlets (invoking twice to make the invlet non-player-assigned)
    game_menus::inv::reassign_letter( dummy, tshirt1, invlet_2 );
    game_menus::inv::reassign_letter( dummy, tshirt1, invlet_2 );

    // drop the items
    move_item( dummy, tshirt1, loc, GROUND );
    move_item( dummy, tshirt2, loc, GROUND );

    // get them again
    move_item( dummy, tshirt1, GROUND, loc );
    move_item( dummy, tshirt2, GROUND, loc );

    std::stringstream ss;
    ss << "1. add two items of the same type to " << location_desc( loc ) <<
       ", and ensure them do not stack" << '\n';
    ss << "2. assign different invlets to the two items" << '\n';
    ss << "3. swap the invlets by assign one of the items with the invlet of the other item" <<
       '\n';
    ss << "4. move the items to " << location_desc( GROUND ) << '\n';
    ss << "5. move the items to " << location_desc( loc ) << " again" << '\n';
    ss << "expect the items to keep their swapped invlets" << '\n';
    if( tshirt1.invlet == invlet_2 && tshirt2.invlet == invlet_1 ) {
        ss << "the items actually keep their swapped invlets" << '\n';
    } else {
        ss << "the items actually does not keep their swapped invlets" << '\n';
    }
    INFO( ss.str() );
    // invlets should not disappear and should still be swapped
    CHECK( tshirt1.invlet == invlet_2 );
    CHECK( tshirt2.invlet == invlet_1 );
    CHECK( check_invlet( dummy, tshirt1, invlet_2 ) == CACHED );
    CHECK( check_invlet( dummy, tshirt2, invlet_1 ) == CACHED );

    // clear invlets
    assign_invlet( dummy, tshirt1, invlet_2, NONE );
    assign_invlet( dummy, tshirt2, invlet_1, NONE );
}

static void merge_invlet_test( player &dummy, inventory_location from )
{
    // invlet to assign
    constexpr char invlet_1 = '{';
    constexpr char invlet_2 = '}';

    // should merge from a place other than the inventory
    REQUIRE( from != INVENTORY );
    // cannot assign invlet to items on the ground
    REQUIRE( from != GROUND );

    for( int id = 0; id < INVLET_STATE_NUM * INVLET_STATE_NUM; ++id ) {
        // how to assign invlet to the first item
        invlet_state first_invlet_state = invlet_state( id % INVLET_STATE_NUM );
        // how to assign invlet to the second item
        invlet_state second_invlet_state = invlet_state( id / INVLET_STATE_NUM );
        // what the invlet should be for the merged stack
        invlet_state expected_merged_invlet_state = first_invlet_state != NONE ? first_invlet_state :
                second_invlet_state;
        char expected_merged_invlet = first_invlet_state != NONE ? invlet_1 : second_invlet_state != NONE ?
                                      invlet_2 : 0;

        // remove all items
        dummy.inv_clear();
        dummy.worn.clear();
        dummy.remove_primary_weapon();
        get_map().i_clear( dummy.bub_pos() );

        // some stackable item
        detached_ptr<item> sd1 = item::spawn( "tshirt" );
        item &tshirt1 = *sd1;
        detached_ptr<item> sd2 = item::spawn( "tshirt" );
        item &tshirt2 = *sd2;

        // add the item
        add_item( dummy, std::move( sd1 ), INVENTORY );
        add_item( dummy, std::move( sd2 ), from );

        // assign the items with invlets
        assign_invlet( dummy, tshirt1, invlet_1, first_invlet_state );
        assign_invlet( dummy, tshirt2, invlet_2, second_invlet_state );

        // merge the second item into inventory
        move_item( dummy, tshirt2, from, INVENTORY );

        item &merged_item = tshirt1;
        invlet_state merged_invlet_state = check_invlet( dummy, merged_item, expected_merged_invlet );
        char merged_invlet = merged_item.invlet;

        std::stringstream ss;
        ss << "1. add two stackable items to the inventory and " << location_desc( from ) << '\n';
        ss << "2. assign " << invlet_state_desc( first_invlet_state ) << " invlet " << invlet_1 <<
           " to the item in the inventory " << '\n';
        ss << "3. assign " << invlet_state_desc( second_invlet_state ) << " invlet " << invlet_2 <<
           " to the " << location_desc( from ) << '\n';
        ss << "4. " << move_action_desc( 0, from, INVENTORY ) << '\n';
        ss << "expect the stack in the inventory to have " << invlet_state_desc(
               expected_merged_invlet_state ) << " invlet " << expected_merged_invlet << '\n';
        ss << "the stack actually has " << invlet_state_desc( merged_invlet_state ) << " invlet " <<
           merged_invlet << '\n';
        INFO( ss.str() );
        REQUIRE( merged_item.typeId() == tshirt1.typeId() );
        CHECK( merged_invlet_state == expected_merged_invlet_state );
        CHECK( merged_invlet == expected_merged_invlet );
    }
}

#define invlet_test_autoletter_off( name, dummy, from, to ) \
    SECTION( std::string( name ) + " (auto letter off)" ) { \
        override_option opt( "AUTO_INV_ASSIGN", "disabled" ); \
        invlet_test( dummy, from, to ); \
    }

#define stack_invlet_test_autoletter_off( name, dummy, from, to ) \
    SECTION( std::string( name ) + " (auto letter off)" ) { \
        override_option opt( "AUTO_INV_ASSIGN", "disabled" ); \
        stack_invlet_test( dummy, from, to ); \
    }

#define swap_invlet_test_autoletter_off( name, dummy, loc ) \
    SECTION( std::string( name ) + " (auto letter off)" ) { \
        override_option opt( "AUTO_INV_ASSIGN", "disabled" ); \
        swap_invlet_test( dummy, loc ); \
    }

#define merge_invlet_test_autoletter_off( name, dummy, from ) \
    SECTION( std::string( name ) + " (auto letter off)" ) { \
        override_option opt( "AUTO_INV_ASSIGN", "disabled" ); \
        merge_invlet_test( dummy, from ); \
    }

TEST_CASE( "Inventory letter test", "[invlet]" )
{
    clear_all_state();
    player &dummy = get_avatar();
    const tripoint_bub_ms spot( 60, 60, 0 );
    dummy.setpos( spot );
    get_map().ter_set( spot, ter_id( "t_dirt" ) );
    get_map().furn_set( spot, furn_id( "f_null" ) );
    if( !dummy.has_trait( trait_debug_storage ) ) {
        dummy.set_mutation( trait_debug_storage );
    }

    invlet_test_autoletter_off( "Picking up items from the ground", dummy, GROUND, INVENTORY );
    invlet_test_autoletter_off( "Wearing items from the ground", dummy, GROUND, WORN );
    invlet_test_autoletter_off( "Wielding and wearing items from the ground", dummy, GROUND,
                                WIELDED_OR_WORN );
    invlet_test_autoletter_off( "Wearing items from inventory", dummy, INVENTORY, WORN );

    stack_invlet_test_autoletter_off( "Wearing item from a stack in inventory", dummy, INVENTORY,
                                      WORN );
    stack_invlet_test_autoletter_off( "Wielding item from a stack in inventory", dummy, INVENTORY,
                                      WIELDED_OR_WORN );

    swap_invlet_test_autoletter_off( "Swapping invlets of two worn items of the same type", dummy,
                                     WORN );

    merge_invlet_test_autoletter_off( "Merging wielded item into an inventory stack", dummy,
                                      WIELDED_OR_WORN );
    merge_invlet_test_autoletter_off( "Merging worn item into an inventory stack", dummy, WORN );
}

static void verify_invlet_consistency( const invlet_favorites &fav )
{
    for( const auto &p : fav.get_invlets_by_id() ) {
        for( const char invlet : p.second ) {
            CHECK( fav.contains( invlet, p.first ) );
        }
    }
}

TEST_CASE( "invlet_favourites_can_erase", "[invlet]" )
{
    clear_all_state();
    invlet_favorites fav;
    fav.set( 'a', itype_id( "a" ) );
    verify_invlet_consistency( fav );
    CHECK( fav.invlets_for( itype_id( "a" ) ) == "a" );
    fav.erase( 'a' );
    verify_invlet_consistency( fav );
    CHECK( fav.invlets_for( itype_id( "a" ) ).empty() );
}

TEST_CASE( "invlet_favourites_removes_clashing_on_insertion", "[invlet]" )
{
    clear_all_state();
    invlet_favorites fav;
    fav.set( 'a', itype_id( "a" ) );
    verify_invlet_consistency( fav );
    CHECK( fav.invlets_for( itype_id( "a" ) ) == "a" );
    CHECK( fav.invlets_for( itype_id( "b" ) ).empty() );
    fav.set( 'a', itype_id( "b" ) );
    verify_invlet_consistency( fav );
    CHECK( fav.invlets_for( itype_id( "a" ) ).empty() );
    CHECK( fav.invlets_for( itype_id( "b" ) ) == "a" );
}

TEST_CASE( "invlet_favourites_retains_order_on_insertion", "[invlet]" )
{
    clear_all_state();
    invlet_favorites fav;
    fav.set( 'a', itype_id( "a" ) );
    fav.set( 'b', itype_id( "a" ) );
    fav.set( 'c', itype_id( "a" ) );
    verify_invlet_consistency( fav );
    CHECK( fav.invlets_for( itype_id( "a" ) ) == "abc" );
    fav.set( 'b', itype_id( "a" ) );
    verify_invlet_consistency( fav );
    CHECK( fav.invlets_for( itype_id( "a" ) ) == "abc" );
}
