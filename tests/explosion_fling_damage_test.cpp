#include "catch/catch.hpp"

#include "avatar.h"
#include "explosion.h"
#include "explosion_queue.h"
#include "game.h"
#include "item.h"
#include "map.h"
#include "map_helpers.h"
#include "monster.h"
#include "options_helpers.h"
#include "state_helpers.h"
#include "type_id.h"

static const mtype_id mon_zombie( "mon_zombie" );

TEST_CASE( "explosion_flung_items_damage_creatures", "[explosion][damage]" )
{
    clear_all_state();
    override_option opt( "OLD_EXPLOSIONS", "false" );
    put_player_underground();
    map &here = get_map();

    const tripoint explosion_center( 30, 30, 0 );
    const tripoint item_pos = explosion_center + tripoint_east;
    const tripoint zombie_pos = item_pos + tripoint_east;

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
