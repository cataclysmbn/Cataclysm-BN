#include "catch/catch.hpp"

#include "avatar.h"
#include "cata_utility.h"
#include "character_id.h"
#include "explosion.h"
#include "explosion_queue.h"
#include "game.h"
#include "item.h"
#include "map.h"
#include "map_helpers.h"
#include "monster.h"
#include "npc.h"
#include "options_helpers.h"
#include "player_helpers.h"
#include "state_helpers.h"
#include "type_id.h"

static const mtype_id mon_zombie( "mon_zombie" );

TEST_CASE( "post_death_explosion_does_not_move_avatar_before_follower_takeover",
           "[explosion][npc][avatar]" )
{
    clear_all_state();
    override_option opt( "OLD_EXPLOSIONS", "false" );
    clear_map();
    move_player_out_of_the_way();

    avatar &you = get_avatar();
    const auto cleanup_test_state = on_out_of_scope( []() {
        clear_all_state();
        get_avatar().setID( character_id(), true );
    } );
    npc &follower = spawn_npc( point_bub_ms( 45, 30 ), "test_talker" );
    follower.set_fac( faction_id( "your_followers" ) );
    follower.set_attitude( NPCATT_FOLLOW );
    REQUIRE( follower.is_player_ally() );

    const auto player_pos = you.bub_pos();
    const auto player_abs = you.abs_pos();
    const auto explosion_center = player_pos;
    const auto follower_abs = player_abs + tripoint_rel_ms( 0, 600, 0 );
    follower.setpos( follower_abs );
    REQUIRE_FALSE( get_map().inbounds( follower.bub_pos() ) );
    you.set_part_hp_cur( bodypart_id( "head" ), 0 );
    you.set_part_hp_cur( bodypart_id( "torso" ), 0 );
    REQUIRE( you.Character::is_dead_state() );
    REQUIRE( you.bub_pos() == player_pos );

    explosion_handler::get_explosion_queue().clear();
    const auto ex = explosion_data{
        .damage = 500,
        .radius = 3.0f,
    };

    explosion_handler::explosion( explosion_center, ex, nullptr );
    explosion_handler::get_explosion_queue().execute();

    REQUIRE( you.Character::is_dead_state() );
    CHECK( you.abs_pos() == player_abs );
    CHECK( follower.abs_pos() == follower_abs );

    you.control_npc( follower );

    CHECK( you.abs_pos() == follower_abs );
    CHECK( you.abs_pos() != player_abs );
    CHECK( get_map().inbounds( you.bub_pos() ) );
    CHECK_FALSE( you.is_dead_state() );
    you.setID( character_id(), true );
}

TEST_CASE( "explosion_flung_items_damage_creatures", "[explosion][damage]" )
{
    clear_all_state();
    override_option opt( "OLD_EXPLOSIONS", "false" );
    move_player_out_of_the_way();
    map &here = get_map();

    const tripoint_bub_ms explosion_center( 30, 30, 0 );
    const auto item_pos = explosion_center + tripoint_east;
    const auto zombie_pos = item_pos + tripoint_east;

    SECTION( "heavy_items_flung_by_explosion_damage_creatures" ) {
        monster &zombie = spawn_test_monster( mon_zombie.str(), zombie_pos );
        const int initial_hp = zombie.get_hp();

        detached_ptr<item> rock = item::spawn( "rock" );
        here.add_item( item_pos, std::move( rock ) );

        const size_t items_at_start = here.i_at( item_pos ).size();
        REQUIRE( items_at_start == 1 );

        explosion_handler::get_explosion_queue().clear();
        const auto ex = explosion_data{
            .damage = 50,
            .radius = 5.0f,
        };

        explosion_handler::explosion( explosion_center, ex, nullptr );
        explosion_handler::get_explosion_queue().execute();

        REQUIRE_FALSE( zombie.is_dead() );

        // Check if items moved
        const size_t items_after = here.i_at( item_pos ).size();
        INFO( "Items at original position: " << items_after );
        INFO( "Items at zombie position: " << here.i_at( zombie_pos ).size() );

        const int hp_after = zombie.get_hp();
        INFO( "HP before: " << initial_hp << ", HP after: " << hp_after );
        CHECK( hp_after < initial_hp );
    }

    SECTION( "nails_flung_by_explosion_damage_creatures" ) {
        monster &zombie = spawn_test_monster( mon_zombie.str(), zombie_pos );
        const int initial_hp = zombie.get_hp();

        for( int i = 0; i < 10; i++ ) {
            detached_ptr<item> nail = item::spawn( "nail" );
            here.add_item( item_pos, std::move( nail ) );
        }

        explosion_handler::get_explosion_queue().clear();
        auto ex = explosion_data{
            .damage = 50,
            .radius = 5.0f,
        };
        explosion_handler::explosion( explosion_center, ex, nullptr );
        explosion_handler::get_explosion_queue().execute();

        const int hp_after = zombie.get_hp();
        CHECK( hp_after < initial_hp );
    }

    SECTION( "throwable_items_flung_by_explosion_damage_creatures" ) {
        monster &zombie = spawn_test_monster( mon_zombie.str(), zombie_pos );
        const int initial_hp = zombie.get_hp();

        detached_ptr<item> throwing_knife = item::spawn( "throwing_knife" );
        here.add_item( item_pos, std::move( throwing_knife ) );

        explosion_handler::get_explosion_queue().clear();
        auto ex = explosion_data{
            .damage = 50,
            .radius = 5.0f,
        };
        explosion_handler::explosion( explosion_center, ex, nullptr );
        explosion_handler::get_explosion_queue().execute();

        const int hp_after = zombie.get_hp();
        INFO( "HP before: " << initial_hp << ", HP after: " << hp_after );
        CHECK( hp_after < initial_hp );
    }
}
