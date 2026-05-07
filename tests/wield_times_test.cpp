#include "catch/catch.hpp"

#include <cstdio>
#include <string>
#include <list>
#include <memory>

#include "avatar.h"
#include "map.h"
#include "map_helpers.h"
#include "coordinates.h"
#include "player.h"
#include "player_helpers.h"
#include "state_helpers.h"
#include "string_formatter.h"
#include "item.h"

static void wield_check_internal( player &dummy, item &the_item,
                                  const char *section_text,
                                  const std::string &var_name, const int expected_cost )
{
    dummy.remove_primary_weapon();
    dummy.set_moves( 1000 );
    const int old_moves = dummy.moves;
    dummy.wield( the_item );
    int move_cost = old_moves - dummy.moves;
    if( expected_cost < 0 ) {
        cata_printf( "    wield_check( %s, dummy, %s, %d );\n", section_text, var_name.c_str(), move_cost );
    } else {
        INFO( "Strength:" << dummy.get_str() );
        int max_cost = expected_cost * 1.1f;
        int min_cost = expected_cost * 0.9f;
        CHECK( move_cost <= max_cost );
        CHECK( move_cost >= min_cost );
    }
}

// As macro, so that we can generate the test cases for easy copypasting
#define wield_check_inv(section_text, dummy, the_item, expected_cost) \
    SECTION( section_text) { \
        detached_ptr<item> det=the_item;\
        item &obj=*det;\
        (dummy).i_add(std::move(det));\
        wield_check_internal(dummy, obj, #section_text, #the_item, generating_cases ? -1 : (expected_cost)); \
    }

#define wield_check_floor(section_text, spot, dummy, the_item, expected_cost) \
    SECTION( section_text) { \
        detached_ptr<item> det=the_item;\
        item &obj=*det;\
        m.add_item(spot, std::move(det));\
        wield_check_internal(dummy, obj, #section_text, #the_item, generating_cases ? -1 : (expected_cost)); \
    }

static void do_test( const bool generating_cases )
{
    player &dummy = get_avatar();
    map &m = get_map();
    const auto spot = dummy.bub_pos();

    dummy.worn.clear();
    dummy.reset_encumbrance();
    wield_check_inv( "Wielding halberd from inventory while unencumbered", dummy,
                     item::spawn( "halberd" ), 287 );
    wield_check_inv( "Wielding 1 aspirin from inventory while unencumbered", dummy,
                     item::spawn( "aspirin", calendar::start_of_cataclysm, 1 ), 100 );
    wield_check_inv( "Wielding combat knife from inventory while unencumbered", dummy,
                     item::spawn( "knife_combat" ), 125 );
    wield_check_floor( "Wielding metal tank from outside inventory while unencumbered", spot, dummy,
                       item::spawn( "metal_tank" ), 300 );
    dummy.worn.clear();
    dummy.worn.push_back( item::spawn( "gloves_work" ) );
    dummy.reset_encumbrance();
    wield_check_inv( "Wielding halberd from inventory while wearing work gloves", dummy,
                     item::spawn( "halberd" ), 307 );
    wield_check_inv( "Wielding 1 aspirin from inventory while wearing work gloves", dummy,
                     item::spawn( "aspirin", calendar::start_of_cataclysm, 1 ), 120 );
    wield_check_inv( "Wielding combat knife from inventory while wearing work gloves", dummy,
                     item::spawn( "knife_combat" ), 150 );
    wield_check_floor( "Wielding metal tank from outside inventory while wearing work gloves", spot,
                       dummy, item::spawn( "metal_tank" ), 340 );
    dummy.worn.clear();
    dummy.worn.push_back( item::spawn( "boxing_gloves" ) );
    dummy.reset_encumbrance();
    wield_check_inv( "Wielding halberd from inventory while wearing boxing gloves", dummy,
                     item::spawn( "halberd" ), 365 );
    wield_check_inv( "Wielding 1 aspirin from inventory while wearing boxing gloves", dummy,
                     item::spawn( "aspirin", calendar::start_of_cataclysm, 1 ), 170 );
    wield_check_inv( "Wielding combat knife from inventory while wearing boxing gloves", dummy,
                     item::spawn( "knife_combat" ), 200 );
    wield_check_floor( "Wielding metal tank from outside inventory while wearing boxing gloves", spot,
                       dummy, item::spawn( "metal_tank" ), 400 );
}

TEST_CASE( "Wield time test", "[wield]" )
{
    clear_all_state();
    do_test( false );
}

TEST_CASE( "Wield time make cases", "[.]" )
{
    clear_all_state();
    do_test( true );
}
