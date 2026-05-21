#include "avatar.h"
#include "character.h"
#include "clzones.h"
#include "itype.h"
#include "player.h"
#include "map.h"
#include "item.h"
#include "activity_handlers.h"
#include "avatar_action.h"
#include "pickup.h"
#include "calendar.h"
#include "player_helpers.h"
#include "state_helpers.h"
#include "vehicle.h"
#include "vehicle_part.h"
#include "type_id.h"

#include "catch/catch.hpp"

/** food items are counted by charges */
static auto get_single_food_item( const tripoint_bub_ms &pos ) -> const item &
{
    map &here = get_map();
    const auto &items = here.i_at( pos );
    CHECK( items.size() == 1 );

    return **items.begin();
}

TEST_CASE( "auto_consume_priority", "[auto_consume][food][zone]" )
{
    clear_all_state();

    map &here = get_map();
    auto &zmgr = zone_manager::get_manager();

    constexpr auto zone_origin = tripoint_bub_ms{ 60, 60, 0 };
    auto zone_origin_absolute = here.bub_to_abs( zone_origin );
    constexpr auto zone_size = tripoint_rel_ms{ 6, 6, 0 };

    avatar &you = get_avatar();
    you.setpos( zone_origin );

    static auto create_zone = [ &, zone_origin_absolute,
       zone_size]( const std::string & name ) -> void {
        zmgr.add( name, zone_type_id( name ),
                  faction_id( "your_followers" ), false, true,
                  zone_origin_absolute - zone_size,
                  zone_origin_absolute + zone_size );
    };

    static auto place_items = [&]( const std::vector<std::pair<item *, tripoint_bub_ms>> &item_pairs )
                              ->
    void {
        for( const auto &[ item, pos ] : item_pairs )
        {
            here.add_item_or_charges( pos, item::spawn( *item ) );
        }
    };

    static const auto auto_consume = [&]( consume_type consume ) {
        return [&you, consume]( int count ) -> bool {
            bool ok = true;
            for( int i = 0; i < count; i++ )
            {
                ok &= find_auto_consume( you, consume );
            }
            return ok;
        };
    };

    using PosCounts = std::vector<std::pair<tripoint_bub_ms, int>>;

    SECTION( "auto_eat" ) {
        static const auto check_item_count =
        [&]( const PosCounts & expected ) -> void {
            for( const auto&[ pos, count ] : expected )
            {
                if( count == 0 ) {
                    INFO( "expected empty at " << pos );
                    CHECK( here.i_at( pos ).empty() );
                } else {
                    INFO( "expected " << count << " at " << pos );
                    CHECK( get_single_food_item( pos ).count() == count );
                }
            }
        };

        static const auto auto_eat = auto_consume( consume_type::FOOD );

        clear_avatar();
        you.set_stored_kcal( 1000 );

        create_zone( "AUTO_EAT" );

        auto meat = item::spawn_temporary( "meat_cooked", calendar::turn, 5 ); // shelf life: 2 days
        auto meat_pos = zone_origin;
        auto nuts = item::spawn_temporary( "pine_nuts", calendar::turn, 5 ); // shelf life: 3 seasons
        auto nuts_pos = zone_origin + tripoint_east;
        auto hardtack = item::spawn_temporary( "hardtack", calendar::turn, 5 ); // shelf life: 6 years
        auto hardtack_pos = zone_origin + tripoint_east * 2;

        place_items( { { meat, meat_pos }, { nuts, nuts_pos }, { hardtack, hardtack_pos } } );

        CHECK( auto_eat( 5 ) );
        check_item_count( { { meat_pos, 0 }, { nuts_pos, 5 }, { hardtack_pos, 5 } } );
        CHECK( auto_eat( 5 ) );
        check_item_count( { { meat_pos, 0 }, { nuts_pos, 0 }, { hardtack_pos, 5 } } );
        CHECK( auto_eat( 5 ) );
        check_item_count( { { meat_pos, 0 }, { nuts_pos, 0 }, { hardtack_pos, 0 } } );

        // check that the player has consumed the food
        CHECK( you.stomach.get_calories() > 1000 );
    }

    SECTION( "auto_drink" ) {
        static const auto check_drink_amount =
        [&]( const PosCounts & expected ) -> void {
            for( const auto&[ pos, count ] : expected )
            {
                auto &jar = get_single_food_item( pos );
                auto &contained = jar.get_contained();
                INFO( contained.tname() << " has " << contained.count() << " charges" );
                if( count == 0 ) {
                    CHECK( contained.is_null() );
                } else {
                    CHECK( contained.count() == count );
                }
            }
        };

        static const auto auto_drink = auto_consume( consume_type::DRINK );

        create_zone( "AUTO_DRINK" );

        auto jar = itype_id{"jar_3l_glass"};
        auto water = item::spawn( "water_clean" );
        auto water_bottle = item::in_container( jar, std::move( water ) );
        auto water_pos = zone_origin;
        auto orange = item::spawn( "oj" ); // 5 days
        auto orange_bottle = item::in_container( jar, std::move( orange ) );
        auto orange_pos = zone_origin +  tripoint_east;
        auto cocoa = item::spawn( "hot_chocolate" ); // 1 day
        auto cocoa_bottle = item::in_container( jar, std::move( cocoa ) );
        auto cocoa_pos = zone_origin +  tripoint_east * 2;

        place_items( { { &*water_bottle, water_pos }, { &*orange_bottle, orange_pos }, { &*cocoa_bottle, cocoa_pos } } );

        SECTION( "full character won't drink drink with calories" ) {
            clear_avatar();
            you.set_stored_kcal( you.max_stored_kcal() );
            you.set_thirst( 700 );

            check_drink_amount( {  { water_pos, 12 }, { orange_pos, 12 }, { cocoa_pos, 12 } } );
            CHECK( auto_drink( 6 ) );
            check_drink_amount( {  { water_pos, 6 }, { orange_pos, 12 }, { cocoa_pos, 12 } } );

            CHECK( you.get_thirst() < 700 );
        }

        SECTION( "hungry character will drink drink with calories" ) {
            clear_avatar();
            you.set_thirst( 700 );
            you.set_stored_kcal( 1000 );

            CHECK( auto_drink( 12 ) );
            INFO( "only cocoa should be consumed" );
            // FIXME: can't figure out why water is replenished, but it is
            check_drink_amount( {  { water_pos, 12 }, { orange_pos, 12 }, { cocoa_pos, 0 } } );

            CHECK( you.get_thirst() < 700 );
        }
    }
}
