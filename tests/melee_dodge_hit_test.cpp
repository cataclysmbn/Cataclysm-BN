#include "catch/catch.hpp"

#include <list>
#include <memory>

#include "avatar.h"
#include "calendar.h"
#include "creature.h"
#include "flag.h"
#include "game.h"
#include "item.h"
#include "map_helpers.h"
#include "monster.h"
#include "mtype.h"
#include "player_helpers.h"
#include "point.h"
#include "state_helpers.h"
#include "type_id.h"

// The test cases below cover polymorphic functions related to melee hit and dodge rates
// for the Character, player, and monster classes, including:
//
// - Character::get_hit_base, monster::get_hit_base
// - Character::get_dodge_base, monster::get_dodge_base
// - Character::get_dodge, monster::get_dodge

// Return the avatar's `get_hit_base` with a given DEX stat.
static float hit_base_with_dex( avatar &dummy, int dexterity )
{
    clear_character( dummy );
    dummy.dex_max = dexterity;

    return dummy.get_hit_base();
}

// Return the avatar's `get_dodge_base` with the given DEX stat and dodge skill.
static float dodge_base_with_dex_and_skill( avatar &dummy, int dexterity, int dodge_skill )
{
    clear_character( dummy );
    dummy.dex_max = dexterity;
    dummy.set_skill_level( skill_id( "dodge" ), dodge_skill );

    return dummy.get_dodge_base();
}

// Return the Creature's `get_dodge` with the given effect.
static float dodge_with_effect( Creature &critter, std::string effect_name )
{
    // Set one effect and leave other attributes alone
    critter.clear_effects();
    critter.add_effect( efftype_id( effect_name ), 1_hours, bodypart_str_id::NULL_ID(), 1, true, true );
    CHECK( critter.has_effect( efftype_id( effect_name ) ) );

    return critter.get_dodge();
}

// Return the avatar's `get_dodge` while wearing a single item of clothing.
static float dodge_wearing_item( avatar &dummy, item &clothing )
{
    // Get nekkid and wear just this one item
    std::vector<detached_ptr<item>> temp;
    while( dummy.takeoff( dummy.i_at( -2 ), &temp ) );
    dummy.wear_item( item::spawn( clothing ) );

    return dummy.get_dodge();
}

TEST_CASE( "monster::get_hit_base", "[monster][melee][hit]" )
{
    clear_all_state();
    SECTION( "monster get_hit_base is equal to melee skill level" ) {
        monster zed( mtype_id( "mon_zombie" ) );
        CHECK( zed.get_hit_base() == zed.type->melee_skill );
    }
}

TEST_CASE( "Character::get_hit_base", "[character][melee][hit][dex]" )
{
    clear_all_state();
    avatar &dummy = g->u;
    clear_character( dummy );

    SECTION( "character get_hit_base increases by 1/4 for each point of DEX" ) {
        CHECK( hit_base_with_dex( dummy, 1 ) == 0.25f );
        CHECK( hit_base_with_dex( dummy, 2 ) == 0.5f );
        CHECK( hit_base_with_dex( dummy, 4 ) == 1.0f );
        CHECK( hit_base_with_dex( dummy, 6 ) == 1.5f );
        CHECK( hit_base_with_dex( dummy, 8 ) == 2.0f );
        CHECK( hit_base_with_dex( dummy, 10 ) == 2.5f );
        CHECK( hit_base_with_dex( dummy, 12 ) == 3.0f );
    }
}

TEST_CASE( "monster::get_dodge_base", "[monster][melee][dodge]" )
{
    clear_all_state();
    SECTION( "monster get_dodge_base is equal to dodge skill level" ) {
        monster smoker( mtype_id( "mon_zombie_smoker" ) );
        CHECK( smoker.get_dodge_base() == smoker.type->sk_dodge );
    }
}

TEST_CASE( "Character::get_dodge_base", "[character][melee][dodge][dex][skill]" )
{
    clear_all_state();
    avatar &dummy = g->u;
    clear_character( dummy );

    // Character::get_dodge_base is simply DEXTERITY / 4 + DODGE_SKILL
    // Even with average dexterity, you can become really good at dodging
    GIVEN( "character has 8 DEX and no dodge skill" ) {
        THEN( "base dodge is 2" ) {
            CHECK( dodge_base_with_dex_and_skill( dummy, 8, 0 ) == 2.0f );
        }

        WHEN( "their dodge skill increases to 2" ) {
            THEN( "base dodge is 4" ) {
                CHECK( dodge_base_with_dex_and_skill( dummy, 8, 2 ) == 4.0f );
            }
        }

        AND_WHEN( "their dodge skill increases to 8" ) {
            THEN( "base dodge is 10" ) {
                CHECK( dodge_base_with_dex_and_skill( dummy, 8, 8 ) == 10.0f );
            }
        }
    }

    // More generally

    SECTION( "character get_dodge_base increases by 1/4 for each point of DEX" ) {
        CHECK( dodge_base_with_dex_and_skill( dummy, 1, 0 ) == 0.25f );
        CHECK( dodge_base_with_dex_and_skill( dummy, 2, 0 ) == 0.5f );
        CHECK( dodge_base_with_dex_and_skill( dummy, 3, 0 ) == 0.75f );
        CHECK( dodge_base_with_dex_and_skill( dummy, 4, 0 ) == 1.0f );
        CHECK( dodge_base_with_dex_and_skill( dummy, 5, 0 ) == 1.25f );
        CHECK( dodge_base_with_dex_and_skill( dummy, 6, 0 ) == 1.5f );
        CHECK( dodge_base_with_dex_and_skill( dummy, 7, 0 ) == 1.75f );
        CHECK( dodge_base_with_dex_and_skill( dummy, 8, 0 ) == 2.0f );
        CHECK( dodge_base_with_dex_and_skill( dummy, 9, 0 ) == 2.25f );
        CHECK( dodge_base_with_dex_and_skill( dummy, 10, 0 ) == 2.50f );
    }

    SECTION( "character get_dodge_base increases by 1 for each level of dodge skill" ) {
        CHECK( dodge_base_with_dex_and_skill( dummy, 8, 0 ) == 2.0f );
        CHECK( dodge_base_with_dex_and_skill( dummy, 8, 1 ) == 3.0f );
        CHECK( dodge_base_with_dex_and_skill( dummy, 8, 2 ) == 4.0f );
        CHECK( dodge_base_with_dex_and_skill( dummy, 8, 3 ) == 5.0f );
        CHECK( dodge_base_with_dex_and_skill( dummy, 8, 4 ) == 6.0f );

        CHECK( dodge_base_with_dex_and_skill( dummy, 6, 2 ) == 3.5f );
        CHECK( dodge_base_with_dex_and_skill( dummy, 6, 4 ) == 5.5f );
        CHECK( dodge_base_with_dex_and_skill( dummy, 6, 6 ) == 7.5f );
        CHECK( dodge_base_with_dex_and_skill( dummy, 6, 8 ) == 9.5f );

        CHECK( dodge_base_with_dex_and_skill( dummy, 10, 7 ) == 9.5f );
        CHECK( dodge_base_with_dex_and_skill( dummy, 10, 8 ) == 10.5f );
        CHECK( dodge_base_with_dex_and_skill( dummy, 10, 9 ) == 11.5f );
        CHECK( dodge_base_with_dex_and_skill( dummy, 10, 10 ) == 12.5f );
    }
}

TEST_CASE( "monster::get_dodge with effects", "[monster][melee][dodge][effect]" )
{
    clear_all_state();
    monster zombie( mtype_id( "mon_zombie_smoker" ) );

    const float base_dodge = zombie.get_dodge_base();
    REQUIRE( base_dodge > 0 );

    // Monsters don't have all the status effects of characters and can't be grabbed,
    // but a few things may affect their ability to dodge.

    SECTION( "no effects or bonuses: base dodge" ) {
        zombie.clear_effects();
        CHECK( zombie.get_dodge() == base_dodge );
    }

    SECTION( "downed: cannot dodge" ) {
        CHECK( dodge_with_effect( zombie, "downed" ) == 0.0f );
    }

    SECTION( "trapped or tied: 1/2 dodge" ) {
        CHECK( dodge_with_effect( zombie, "beartrap" ) == base_dodge / 2 );
        CHECK( dodge_with_effect( zombie, "tied" ) == base_dodge / 2 );
    }

    SECTION( "unstable footing: 1/4 dodge" ) {
        CHECK( dodge_with_effect( zombie, "bouldering" ) == base_dodge / 4 );
    }
}

TEST_CASE( "Character::get_dodge", "[player][melee][dodge]" )
{
    clear_all_state();
    avatar &dummy = g->u;
    clear_character( dummy );

    const float base_dodge = dummy.get_dodge_base();

    SECTION( "each dodge after the first subtracts 2 points" ) {
        // Simulate a dodge
        dummy.on_dodge( nullptr, 0 );
        CHECK( dummy.get_dodge() == base_dodge - 2 );

        // Reset dodges_left, so subsequent tests are not affected
        dummy.set_moves( 100 );
        dummy.process_turn();
        REQUIRE( dummy.dodges_left > 0 );
    }
}

TEST_CASE( "Character::get_dodge with effects", "[player][melee][dodge][effect]" )
{
    clear_all_state();
    avatar &dummy = g->u;
    clear_character( dummy );

    // Compare all effects against base dodge ability
    const float base_dodge = dummy.get_dodge_base();

    SECTION( "no effects or bonuses: base dodge" ) {
        dummy.clear_effects();
        CHECK( dummy.get_dodge() == base_dodge );
    }

    SECTION( "unconscious or winded: cannot dodge" ) {
        CHECK( dodge_with_effect( dummy, "sleep" ) == 0.0f );
        CHECK( dodge_with_effect( dummy, "lying_down" ) == 0.0f );
        CHECK( dodge_with_effect( dummy, "npc_suspend" ) == 0.0f );
        CHECK( dodge_with_effect( dummy, "narcosis" ) == 0.0f );
    }

    SECTION( "trapped: 1/2 dodge" ) {
        CHECK( dodge_with_effect( dummy, "beartrap" ) == base_dodge / 2 );
    }

    SECTION( "unstable footing: 1/4 dodge" ) {
        CHECK( dodge_with_effect( dummy, "bouldering" ) == base_dodge / 4 );
    }

    SECTION( "skating: amateur or pro?" ) {
        item &skates = *item::spawn_temporary( "rollerskates" );
        item &blades = *item::spawn_temporary( "roller_blades" );
        item &heelys = *item::spawn_temporary( "roller_shoes_on" );

        REQUIRE( skates.has_flag( flag_ROLLER_QUAD ) );
        REQUIRE( blades.has_flag( flag_ROLLER_INLINE ) );
        REQUIRE( heelys.has_flag( flag_ROLLER_ONE ) );

        SECTION( "amateur skater: 1/5 dodge" ) {
            REQUIRE_FALSE( dummy.has_trait( trait_id( "PROF_SKATER" ) ) );

            CHECK( dodge_wearing_item( dummy, skates ) == base_dodge / 5 );
            CHECK( dodge_wearing_item( dummy, blades ) == base_dodge / 5 );
            CHECK( dodge_wearing_item( dummy, heelys ) == base_dodge / 5 );
        }

        SECTION( "professional skater: 1/2 dodge" ) {
            dummy.toggle_trait( trait_id( "PROF_SKATER" ) );
            REQUIRE( dummy.has_trait( trait_id( "PROF_SKATER" ) ) );

            CHECK( dodge_wearing_item( dummy, skates ) == base_dodge / 2 );
            CHECK( dodge_wearing_item( dummy, blades ) == base_dodge / 2 );
            CHECK( dodge_wearing_item( dummy, heelys ) == base_dodge / 2 );
        }
    }
}

TEST_CASE( "player::get_dodge while grabbed", "[player][melee][dodge][grab]" )
{
    clear_all_state();
    avatar &dummy = g->u;
    clear_character( dummy );

    // Base dodge rate when not grabbed
    const float base_dodge = dummy.get_dodge_base();

    // Four nearby spots
    auto mon1_pos = dummy.bub_pos() + tripoint_north;
    auto mon2_pos = dummy.bub_pos() + tripoint_east;
    auto mon3_pos = dummy.bub_pos() + tripoint_south;
    auto mon4_pos = dummy.bub_pos() + tripoint_west;

    // Surrounded by zombies!
    monster *zed1 = g->place_critter_at( mtype_id( "debug_mon" ), mon1_pos );
    monster *zed2 = g->place_critter_at( mtype_id( "debug_mon" ), mon2_pos );
    monster *zed3 = g->place_critter_at( mtype_id( "debug_mon" ), mon3_pos );
    monster *zed4 = g->place_critter_at( mtype_id( "debug_mon" ), mon4_pos );

    // Make sure zombies are in their places
    REQUIRE( g->critter_at<monster>( mon1_pos ) );
    REQUIRE( g->critter_at<monster>( mon2_pos ) );
    REQUIRE( g->critter_at<monster>( mon3_pos ) );
    REQUIRE( g->critter_at<monster>( mon4_pos ) );

    // Get grabbed
    dummy.add_effect( efftype_id( "grabbed" ), 1_minutes );
    REQUIRE( dummy.has_effect( efftype_id( "grabbed" ) ) );

    // When grabbed, dodge skill reduces for each additional grab

    SECTION( "1 grab: 75% dodge" ) {
        zed1->add_effect( efftype_id( "grabbing" ), 1_minutes );
        REQUIRE( zed1->has_effect( efftype_id( "grabbing" ) ) );

        CHECK( dummy.get_dodge() == base_dodge * 0.75f );
    }

    SECTION( "2 grabs: 50% dodge" ) {
        zed1->add_effect( efftype_id( "grabbing" ), 1_minutes );
        zed2->add_effect( efftype_id( "grabbing" ), 1_minutes );
        REQUIRE( zed1->has_effect( efftype_id( "grabbing" ) ) );
        REQUIRE( zed2->has_effect( efftype_id( "grabbing" ) ) );

        CHECK( dummy.get_dodge() == base_dodge * 0.5f );
    }

    SECTION( "3 grabs: 25% dodge" ) {
        zed1->add_effect( efftype_id( "grabbing" ), 1_minutes );
        zed2->add_effect( efftype_id( "grabbing" ), 1_minutes );
        zed3->add_effect( efftype_id( "grabbing" ), 1_minutes );
        REQUIRE( zed1->has_effect( efftype_id( "grabbing" ) ) );
        REQUIRE( zed2->has_effect( efftype_id( "grabbing" ) ) );
        REQUIRE( zed3->has_effect( efftype_id( "grabbing" ) ) );

        CHECK( dummy.get_dodge() == base_dodge * 0.25f );
    }

    SECTION( "4 grabs: 0% dodge" ) {
        zed1->add_effect( efftype_id( "grabbing" ), 1_minutes );
        zed2->add_effect( efftype_id( "grabbing" ), 1_minutes );
        zed3->add_effect( efftype_id( "grabbing" ), 1_minutes );
        zed4->add_effect( efftype_id( "grabbing" ), 1_minutes );
        REQUIRE( zed1->has_effect( efftype_id( "grabbing" ) ) );
        REQUIRE( zed2->has_effect( efftype_id( "grabbing" ) ) );
        REQUIRE( zed3->has_effect( efftype_id( "grabbing" ) ) );
        REQUIRE( zed4->has_effect( efftype_id( "grabbing" ) ) );

        CHECK( dummy.get_dodge() == 0.0f );
    }
}
