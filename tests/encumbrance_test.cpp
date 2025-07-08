#include "catch/catch.hpp"

#include <array>
#include <functional>
#include <list>
#include <memory>
#include <string>
#include <vector>

#include "avatar.h"
#include "bodypart.h"
#include "character.h"
#include "character_encumbrance.h"
#include "game.h"
#include "item.h"
#include "material.h"
#include "npc.h"
#include "player.h"
#include "state_helpers.h"
#include "type_id.h"

static void test_encumbrance_on(
    player &p,
    std::vector<detached_ptr<item>> &clothing,
    const bodypart_str_id &body_part,
    int expected_encumbrance,
    const std::function<void( player & )> &tweak_player = {}
)
{
    CAPTURE( body_part );
    p.set_body();
    p.clear_mutations();
    p.worn.clear();
    if( tweak_player ) {
        tweak_player( p );
    }
    for( detached_ptr<item> &i : clothing ) {
        p.worn.push_back( std::move( i ) );
    }
    p.reset_encumbrance();
    encumbrance_data enc = p.get_encumbrance().elems[ body_part ];
    CHECK( enc.encumbrance == expected_encumbrance );
}

static void test_encumbrance_items(
    std::vector<detached_ptr<item>> &clothing,
    const bodypart_str_id &body_part,
    const int expected_encumbrance,
    const std::function<void( player & )> &tweak_player = {}
)
{
    // Test NPC first because NPC code can accidentally end up using properties
    // of g->u, and such bugs are hidden if we test the other way around.
    SECTION( "testing on npc" ) {
        npc example_npc;
        test_encumbrance_on( example_npc, clothing, body_part, expected_encumbrance, tweak_player );
    }
    SECTION( "testing on player" ) {
        test_encumbrance_on( get_avatar(), clothing, body_part, expected_encumbrance, tweak_player );
    }
}

static void test_encumbrance(
    const std::vector<std::string> &clothing_types,
    const bodypart_str_id &body_part,
    const int expected_encumbrance
)
{
    CAPTURE( clothing_types );
    std::vector<detached_ptr<item>> clothing;
    for( const std::string &type : clothing_types ) {
        clothing.push_back( item::spawn( type ) );
    }
    test_encumbrance_items( clothing, body_part, expected_encumbrance );
}

struct add_trait {
    add_trait( const std::string &t ) : trait( t ) {}
    add_trait( const trait_id &t ) : trait( t ) {}

    void operator()( player &p ) {
        p.toggle_trait( trait );
    }

    trait_id trait;
};

static constexpr int karate_gi_e = 0;
static constexpr int vest_e = 5;
static constexpr int greatcoat_e = 23;

TEST_CASE( "regular_clothing_encumbrance", "[encumbrance]" )
{
    clear_all_state();
    test_encumbrance( { "karate_gi" }, body_part_torso, karate_gi_e );
    test_encumbrance( { "vest" }, body_part_torso, vest_e );
    test_encumbrance( { "greatcoat" }, body_part_torso, greatcoat_e );
}

TEST_CASE( "separate_layer_encumbrance", "[encumbrance]" )
{
    clear_all_state();
    test_encumbrance( { "vest", "greatcoat" }, body_part_torso, vest_e + greatcoat_e );
}

TEST_CASE( "out_of_order_encumbrance", "[encumbrance]" )
{
    clear_all_state();
    test_encumbrance( { "greatcoat", "vest" }, body_part_torso, vest_e * 2 + greatcoat_e );
}

TEST_CASE( "same_layer_encumbrance", "[encumbrance]" )
{
    clear_all_state();
    // When stacking within a layer, encumbrance for additional items is
    // counted twice
    test_encumbrance( { "vest", "vest" }, body_part_torso, vest_e * 2 + vest_e );
    // ... with a minimum of 2
    test_encumbrance( { "karate_gi", "karate_gi" }, body_part_torso, karate_gi_e * 2 + 2 );
    // ... and a maximum of 10
    test_encumbrance( { "greatcoat", "greatcoat" }, body_part_torso, greatcoat_e * 2 + 10 );
}

TEST_CASE( "tiny_clothing", "[encumbrance]" )
{
    clear_all_state();
    detached_ptr<item> i = item::spawn( "vest" );
    i->set_flag( flag_id( "UNDERSIZE" ) );
    std::vector<detached_ptr<item>> items;
    items.push_back( std::move( i ) );
    test_encumbrance_items( items, body_part_torso,
                            vest_e * 3 );
}

TEST_CASE( "tiny_character", "[encumbrance]" )
{
    clear_all_state();
    detached_ptr<item> i = item::spawn( "vest" );
    item &obj = *i;
    std::vector<detached_ptr<item>> items;
    items.push_back( std::move( i ) );
    SECTION( "regular shirt" ) {
        test_encumbrance_items( items, body_part_torso,
                                vest_e * 2,
                                add_trait( "SMALL2" ) );
    }
    SECTION( "undersize shrt" ) {
        obj.set_flag( flag_id( "UNDERSIZE" ) );
        test_encumbrance_items( items, body_part_torso, vest_e,
                                add_trait( "SMALL2" ) );
    }
}
