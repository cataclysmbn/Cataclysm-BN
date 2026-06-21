#include "catch/catch.hpp"

#include <list>
#include <memory>
#include <string>

#include "avatar.h"
#include "bodypart.h"
#include "calendar.h"
#include "creature.h"
#include "game.h"
#include "item.h"
#include "map_helpers.h"
#include "monster.h"
#include "mtype.h"
#include "options_helpers.h"
#include "player_helpers.h"
#include "point.h"
#include "state_helpers.h"
#include "type_id.h"

// The test cases below cover polymorphic functions related to melee hit and dodge rates
// for the Character, player, and monster classes, including:
//
// - Character::get_hit_base, monster::get_hit_base
// - Character::get_dodge_base, monster::get_dodge_base
// - player::get_dodge, monster::get_dodge

TEST_CASE( "Monster losing grabbing effect", "[player][melee][grab]" )
{
    clear_all_state();
    avatar &dummy = g->u;
    clear_character( dummy );

    // Four nearby spots
    auto mon_pos = dummy.bub_pos() + tripoint_north;

    // Grabbed by a monster
    monster *zed = g->place_critter_at( mtype_id( "debug_mon" ), mon_pos );

    REQUIRE( g->critter_at<monster>( mon_pos ) );

    // Get grabbed
    dummy.add_effect( efftype_id( "grabbed" ), 1_minutes );
    REQUIRE( dummy.has_effect( efftype_id( "grabbed" ) ) );

    zed->add_effect( efftype_id( "grabbing" ), 1_minutes );
    REQUIRE( zed->has_effect( efftype_id( "grabbing" ) ) );

    SECTION( "Monster is bashed for over 10% its hp" ) {
        damage_type dt = DT_BASH;
        bodypart_str_id bp( "torso" );
        damage_instance di( dt, ( zed->get_hp_max() / 5.0 ) + zed->get_armor_type( dt, bp ) );
        zed->deal_damage( nullptr, bp, di );
        REQUIRE( zed->get_hp() <= zed->get_hp_max() * 9 / 10 );

        THEN( "Monster is no longer grabbing" ) {
            CHECK( !zed->has_effect( efftype_id( "grabbing" ) ) );
        }
    }

    SECTION( "Monster is burned for over 10% its hp" ) {
        damage_type dt = DT_HEAT;
        bodypart_str_id bp( "torso" );
        damage_instance di( dt, ( zed->get_hp_max() / 5.0 ) + zed->get_armor_type( dt, bp ) );
        zed->deal_damage( nullptr, bp, di );
        REQUIRE( zed->get_hp() <= zed->get_hp_max() * 9 / 10 );

        THEN( "Monster is still grabbing" ) {
            CHECK( zed->has_effect( efftype_id( "grabbing" ) ) );
        }
    }

    SECTION( "Monster is bashed for 0 damage" ) {
        damage_type dt = DT_BASH;
        damage_instance di( dt, 0 );
        bodypart_str_id bp( "torso" );
        zed->deal_damage( nullptr, bp, di );
        REQUIRE( zed->get_hp() == zed->get_hp_max() );

        THEN( "Monster is still grabbing" ) {
            CHECK( zed->has_effect( efftype_id( "grabbing" ) ) );
        }
    }
}

TEST_CASE( "Avatar drags manually grabbed monster while moving", "[player][melee][grab]" )
{
    clear_all_state();
    avatar &dummy = g->u;
    clear_character( dummy );

    const auto effect_grabbed = efftype_id( "grabbed" );
    const auto effect_grabbing = efftype_id( "grabbing" );
    const auto avatar_start = dummy.bub_pos();
    const auto monster_start = avatar_start + tripoint_north;
    auto &zed = spawn_test_monster( "debug_mon", monster_start );

    dummy.add_effect( effect_grabbing, 1_days, body_part_torso );
    zed.add_effect( effect_grabbed, 1_days );

    REQUIRE( dummy.has_effect( effect_grabbing ) );
    REQUIRE( zed.has_effect( effect_grabbed ) );
    REQUIRE( g->critter_at<monster>( monster_start ) == &zed );

    const auto avatar_destination = avatar_start + tripoint_east;
    REQUIRE( g->walk_move( avatar_destination, false ) );

    CHECK( dummy.bub_pos() == avatar_destination );
    CHECK( zed.bub_pos() == avatar_start );
    CHECK( g->critter_at<monster>( avatar_start ) == &zed );
}

TEST_CASE( "Manually grabbed monster cannot walk away", "[player][melee][grab]" )
{
    clear_all_state();
    avatar &dummy = g->u;
    clear_character( dummy );

    const auto effect_grabbed = efftype_id( "grabbed" );
    const auto effect_grabbing = efftype_id( "grabbing" );

    const auto avatar_start = dummy.bub_pos();
    const auto monster_start = avatar_start + tripoint_north;
    const auto monster_destination = monster_start + tripoint_north;
    auto &zed = spawn_test_monster( "debug_mon", monster_start );

    dummy.add_effect( effect_grabbing, 1_days, body_part_torso );
    zed.add_effect( effect_grabbed, 1_days );

    REQUIRE( zed.has_effect( effect_grabbed ) );
    CHECK_FALSE( zed.can_move_to( monster_destination ) );
    CHECK( zed.can_move_to( monster_start ) );
    CHECK_FALSE( zed.move_to( monster_destination ) );
    CHECK( zed.bub_pos() == monster_start );
}

TEST_CASE( "Crowd crush does not drain breath when disabled", "[player][melee][grab]" )
{
    const override_option crowd_crush( "CROWD_CRUSH", "false" );
    clear_all_state();
    avatar &dummy = g->u;
    clear_character( dummy );

    const auto effect_grabbed = efftype_id( "grabbed" );
    const auto effect_grabbing = efftype_id( "grabbing" );
    const auto avatar_start = dummy.bub_pos();

    dummy.add_effect( effect_grabbed, 1_days, body_part_torso );
    dummy.oxygen = 30;

    for( const auto &offset : { tripoint_north, tripoint_south, tripoint_east } ) {
        auto &zed = spawn_test_monster( "debug_mon", avatar_start + offset );
        zed.add_effect( effect_grabbing, 1_days );
    }

    dummy.suffer();

    CHECK( dummy.oxygen == 30 );
}

TEST_CASE( "Crowd crush drains breath while grabbed", "[player][melee][grab]" )
{
    const override_option crowd_crush( "CROWD_CRUSH", "true" );
    clear_all_state();
    avatar &dummy = g->u;
    clear_character( dummy );

    const auto effect_grabbed = efftype_id( "grabbed" );
    const auto effect_grabbing = efftype_id( "grabbing" );
    const auto avatar_start = dummy.bub_pos();

    dummy.add_effect( effect_grabbed, 1_days, body_part_torso );
    dummy.oxygen = 30;

    for( const auto &offset : { tripoint_north, tripoint_south, tripoint_east } ) {
        auto &zed = spawn_test_monster( "debug_mon", avatar_start + offset );
        zed.add_effect( effect_grabbing, 1_days );
    }

    dummy.suffer();

    CHECK( dummy.oxygen < 30 );
}
