#include "catch/catch.hpp"

#include "avatar.h"
#include "character_effects.h"
#include "player_helpers.h"
#include "map.h"
#include "map_helpers.h"
#include "state_helpers.h"

static const trait_id trait_DEBUG_WEIGHTLESSNESS( "DEBUG_WEIGHTLESSNESS" );

static void advance_turn( Character &guy )
{
    guy.process_turn();
    calendar::turn += 1_turns;
}

static player &prepare_player()
{
    player &guy = *get_player_character().as_player();
    clear_character( *guy.as_player(), true );
    guy.set_moves( 0 );

    advance_turn( guy );

    REQUIRE( guy.get_speed_base() == 100 );
    REQUIRE( guy.get_speed() == 100 );
    REQUIRE( guy.get_moves() == 100 );

    guy.set_moves( 0 );

    return guy;
}

TEST_CASE( "Character regains moves each turn", "[speed]" )
{
    clear_all_state();
    player &guy = prepare_player();

    advance_turn( guy );

    CHECK( guy.get_moves() == 100 );
}

static void pain_penalty_test( player &guy, int pain, int speed_exp )
{
    int penalty = 100 - speed_exp;

    guy.set_pain( pain );
    REQUIRE( guy.get_pain() == pain );
    guy.set_painkiller( 0 );
    REQUIRE( guy.get_painkiller() == 0 );
    REQUIRE( guy.get_perceived_pain() == pain );
    REQUIRE( character_effects::get_pain_penalty( guy ).speed == penalty );

    advance_turn( guy );

    CHECK( guy.get_speed_bonus() == -penalty );
    CHECK( guy.get_speed() == speed_exp );
    CHECK( guy.get_moves() == speed_exp );
}

TEST_CASE( "Character is slowed down by pain", "[speed][pain]" )
{
    clear_all_state();
    player &guy = prepare_player();

    WHEN( "10 pain" ) {
        pain_penalty_test( guy, 10, 95 );
    }
    WHEN( "100 pain" ) {
        pain_penalty_test( guy, 100, 75 );
    }
    WHEN( "300 pain" ) {
        pain_penalty_test( guy, 300, 70 );
    }
}

static void carry_weight_test( Character &guy, int load_kg, int speed_exp )
{
    item &item_1kg = *item::spawn_temporary( "test_1kg_cube" );
    REQUIRE( item_1kg.weight() == 1_kilogram );
    REQUIRE( item_1kg.volume() == 10_ml );

    CAPTURE( load_kg, speed_exp );
    WHEN( "Character carries specified weight" ) {
        for( int i = 0; i < load_kg; i++ ) {
            guy.i_add( item::spawn( item_1kg ) );
        }
        THEN( "No effect on speed" ) {
            CHECK( guy.get_speed() == 100 );
            AND_WHEN( "Turn passes" ) {
                advance_turn( guy );
                REQUIRE( guy.weight_carried() == 1_kilogram * load_kg + 633_gram );
                REQUIRE( guy.weight_capacity() == 45_kilogram );
                THEN( "Speed matches expected value" ) {
                    CHECK( guy.get_speed() == speed_exp );
                }
            }
        }
    }
}

static auto equip_carried_weight( Character &guy ) -> units::mass
{
    auto backpack = item::spawn( "test_backpack" );
    auto inventory_item = item::spawn( "test_1kg_cube" );
    auto wielded_item = item::spawn( "test_1kg_cube" );

    const auto carried_weight = backpack->weight() + inventory_item->weight() + wielded_item->weight();

    guy.wear_item( std::move( backpack ), false );
    guy.i_add( std::move( inventory_item ) );
    guy.wield( std::move( wielded_item ) );

    return carried_weight;
}

TEST_CASE( "Character is slowed down while overburdened", "[speed]" )
{
    clear_all_state();
    player &guy = prepare_player();

    detached_ptr<item> backpack = item::spawn( "test_backpack" );
    REQUIRE( backpack->get_storage() == 15_liter );
    REQUIRE( backpack->weight() == 633_gram );

    guy.clear_mutations();
    guy.wear_item( std::move( backpack ), false );
    REQUIRE( guy.weight_capacity() == 45_kilogram );
    REQUIRE( guy.volume_capacity() == 15_liter );

    SECTION( "Carry weight under carry capacity" ) {
        // 94%, no penalty
        carry_weight_test( guy, 42, 100 );
    }
    SECTION( "Carry weight over carry capacity" ) {
        // 148% gives -12 speed (25 * 0.48)
        carry_weight_test( guy, 66, 88 );
    }
    SECTION( "Carry weight significantly over carry capacity" ) {
        // 228% gives -32 speed (25 * 1.28)
        carry_weight_test( guy, 102, 68 );
    }
}

TEST_CASE( "Debug weightlessness ignores carried item weight", "[speed][debug]" )
{
    clear_all_state();
    player &guy = *get_player_character().as_player();

    SECTION( "Normal carrying adds to character weight" ) {
        clear_character( guy, false );
        const auto base_weight = guy.get_weight();
        const auto carried_weight = equip_carried_weight( guy );

        CHECK( guy.get_weight() - base_weight == carried_weight );
    }

    SECTION( "Debug carrying capacity still adds to character weight" ) {
        clear_character( guy, true );
        const auto base_weight = guy.get_weight();
        const auto carried_weight = equip_carried_weight( guy );

        CHECK( guy.get_weight() - base_weight == carried_weight );
    }

    SECTION( "Debug weightlessness zeroes character weight" ) {
        clear_character( guy, true );
        guy.set_mutation( trait_DEBUG_WEIGHTLESSNESS );
        const auto carried_weight = equip_carried_weight( guy );
        CAPTURE( carried_weight );

        CHECK( guy.get_weight() == 0_gram );
    }
}
