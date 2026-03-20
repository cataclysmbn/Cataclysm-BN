#include "catch/catch.hpp"

#include <ranges>
#include <string>

#include "calendar.h"
#include "item.h"
#include "itype.h"
#include "proc_fact.h"
#include "proc_item.h"

namespace
{

auto make_proc_test_item( const itype_id &id, const proc::schema_id &schema,
                          const std::string &fp ) -> detached_ptr<item>
{
    auto crafted = item::spawn( id, calendar::turn );
    auto payload = proc::payload{};
    payload.id = schema;
    payload.fp = fp;
    payload.blob.name = crafted->type_name();
    proc::write_payload( *crafted, payload );
    return crafted;
}

} // namespace

TEST_CASE( "proc_part_fact_normalizes_basic_item_fields", "[proc][fact]" )
{
    const auto apple = item( "apple" );
    const auto fact = proc::normalize_part_fact( apple, { .ix = 3 } );

    CHECK( fact.valid() );
    CHECK( fact.ix == 3 );
    CHECK( fact.id == itype_id( "apple" ) );
    CHECK_FALSE( fact.mat.empty() );
    CHECK( fact.mass_g > 0 );
    CHECK( fact.volume_ml > 0 );
    CHECK( fact.kcal == apple.get_comestible()->default_nutrition.kcal );
    CHECK( fact.hp == Approx( 1.0f ) );
}

TEST_CASE( "proc_part_fact_normalizes_damage_and_charges", "[proc][fact]" )
{
    auto shirt = item( "jeans" );
    REQUIRE( shirt.max_damage() > 0 );
    shirt.set_damage( 1 );

    const auto damaged_fact = proc::normalize_part_fact( shirt, { .ix = 1 } );
    CHECK( damaged_fact.hp < 1.0f );

    const auto battery = item( "battery" );
    const auto battery_fact = proc::normalize_part_fact( battery, {
        .ix = 2,
        .charges = 1,
        .uses = battery.charges
    } );
    CHECK( battery_fact.chg == 1 );
    CHECK( battery_fact.uses == battery.charges );
}

TEST_CASE( "proc_part_fact_assigns_food_role_tags", "[proc][fact]" )
{
    const auto bread = item( "bread" );
    const auto bread_fact = proc::normalize_part_fact( bread, { .ix = 1 } );
    CHECK( std::ranges::find( bread_fact.tag, "bread" ) != bread_fact.tag.end() );

    const auto biscuit = item( "biscuit" );
    const auto biscuit_fact = proc::normalize_part_fact( biscuit, { .ix = 9 } );
    CHECK( std::ranges::find( biscuit_fact.tag, "bread" ) != biscuit_fact.tag.end() );

    const auto hardtack = item( "hardtack" );
    const auto hardtack_fact = proc::normalize_part_fact( hardtack, { .ix = 10 } );
    CHECK( std::ranges::find( hardtack_fact.tag, "bread" ) != hardtack_fact.tag.end() );

    const auto meat = item( "meat_cooked" );
    const auto meat_fact = proc::normalize_part_fact( meat, { .ix = 2 } );
    CHECK( std::ranges::find( meat_fact.tag, "meat" ) != meat_fact.tag.end() );

    const auto rollmat = item( "rollmat" );
    const auto rollmat_fact = proc::normalize_part_fact( rollmat, { .ix = 3 } );
    CHECK( std::ranges::find( rollmat_fact.tag, "bread" ) == rollmat_fact.tag.end() );

    const auto fertilizer = item( "fertilizer" );
    const auto fertilizer_fact = proc::normalize_part_fact( fertilizer, { .ix = 4 } );
    CHECK( std::ranges::find( fertilizer_fact.tag, "veg" ) == fertilizer_fact.tag.end() );
}

TEST_CASE( "proc_part_fact_assigns_cond_tags_to_supported_sandwich_condiments", "[proc][fact]" )
{
    const auto butter_fact = proc::normalize_part_fact( item( "butter" ), { .ix = 13 } );
    CHECK( std::ranges::find( butter_fact.tag, "cond" ) != butter_fact.tag.end() );

    const auto horseradish_fact = proc::normalize_part_fact( item( "horseradish" ), { .ix = 14 } );
    CHECK( std::ranges::find( horseradish_fact.tag, "cond" ) != horseradish_fact.tag.end() );

    const auto sauerkraut_fact = proc::normalize_part_fact( item( "sauerkraut" ), { .ix = 15 } );
    CHECK( std::ranges::find( sauerkraut_fact.tag, "cond" ) != sauerkraut_fact.tag.end() );

    const auto honey_fact = proc::normalize_part_fact( item( "honey_bottled" ), { .ix = 16 } );
    CHECK( std::ranges::find( honey_fact.tag, "cond" ) != honey_fact.tag.end() );

    const auto jam_fact = proc::normalize_part_fact( item( "jam_fruit" ), { .ix = 17 } );
    CHECK( std::ranges::find( jam_fact.tag, "cond" ) != jam_fact.tag.end() );

    const auto peanut_butter_fact = proc::normalize_part_fact( item( "peanutbutter" ), { .ix = 18 } );
    CHECK( std::ranges::find( peanut_butter_fact.tag, "cond" ) != peanut_butter_fact.tag.end() );

    const auto syrup_fact = proc::normalize_part_fact( item( "syrup" ), { .ix = 19 } );
    CHECK( std::ranges::find( syrup_fact.tag, "cond" ) != syrup_fact.tag.end() );
}

TEST_CASE( "proc_part_fact_does_not_tag_non_cheese_dairy_as_cheese", "[proc][fact]" )
{
    const auto butter_fact = proc::normalize_part_fact( item( "butter" ), { .ix = 22 } );
    CHECK( std::ranges::find( butter_fact.tag, "cheese" ) == butter_fact.tag.end() );

    const auto milk_fact = proc::normalize_part_fact( item( "milk" ), { .ix = 23 } );
    CHECK( std::ranges::find( milk_fact.tag, "cheese" ) == milk_fact.tag.end() );

    const auto cheese_fact = proc::normalize_part_fact( item( "cheese" ), { .ix = 24 } );
    CHECK( std::ranges::find( cheese_fact.tag, "cheese" ) != cheese_fact.tag.end() );
}

TEST_CASE( "proc_part_fact_does_not_tag_condiment_spreads_as_veg", "[proc][fact]" )
{
    const auto ketchup_fact = proc::normalize_part_fact( item( "ketchup" ), { .ix = 25 } );
    CHECK( std::ranges::find( ketchup_fact.tag, "cond" ) != ketchup_fact.tag.end() );
    CHECK( std::ranges::find( ketchup_fact.tag, "veg" ) == ketchup_fact.tag.end() );

    const auto carrot_fact = proc::normalize_part_fact( item( "carrot" ), { .ix = 26 } );
    CHECK( std::ranges::find( carrot_fact.tag, "veg" ) != carrot_fact.tag.end() );
}

TEST_CASE( "proc_part_fact_does_not_treat_finished_sandwiches_as_raw_ingredients", "[proc][fact]" )
{
    const auto cheese_sandwich = item( "sandwich_cheese" );
    const auto cheese_fact = proc::normalize_part_fact( cheese_sandwich, { .ix = 5 } );
    CHECK( std::ranges::find( cheese_fact.tag, "bread" ) == cheese_fact.tag.end() );
    CHECK( std::ranges::find( cheese_fact.tag, "cheese" ) == cheese_fact.tag.end() );

    const auto meat_sandwich = item( "sandwich_t" );
    const auto meat_fact = proc::normalize_part_fact( meat_sandwich, { .ix = 6 } );
    CHECK( std::ranges::find( meat_fact.tag, "meat" ) == meat_fact.tag.end() );

    const auto veg_sandwich = item( "sandwich_veggy" );
    const auto veg_fact = proc::normalize_part_fact( veg_sandwich, { .ix = 7 } );
    CHECK( std::ranges::find( veg_fact.tag, "veg" ) == veg_fact.tag.end() );

    const auto sauce_sandwich = item( "sandwich_sauce" );
    const auto sauce_fact = proc::normalize_part_fact( sauce_sandwich, { .ix = 8 } );
    CHECK( std::ranges::find( sauce_fact.tag, "cond" ) == sauce_fact.tag.end() );
}

TEST_CASE( "proc_part_fact_marks_finished_stews_and_curries_as_dishes", "[proc][fact]" )
{
    const auto veggie_soup = item( "soup_veggy" );
    const auto soup_fact = proc::normalize_part_fact( veggie_soup, { .ix = 11 } );
    CHECK( std::ranges::find( soup_fact.tag, "dish" ) != soup_fact.tag.end() );
    CHECK( std::ranges::find( soup_fact.tag, "veg" ) == soup_fact.tag.end() );

    const auto meat_curry = item( "curry_meat" );
    const auto curry_fact = proc::normalize_part_fact( meat_curry, { .ix = 12 } );
    CHECK( std::ranges::find( curry_fact.tag, "dish" ) != curry_fact.tag.end() );
    CHECK( std::ranges::find( curry_fact.tag, "veg" ) == curry_fact.tag.end() );
    CHECK( std::ranges::find( curry_fact.tag, "meat" ) == curry_fact.tag.end() );
}

TEST_CASE( "proc_part_fact_treats_payload_marked_raw_food_as_finished_dish", "[proc][fact]" )
{
    const auto proc_bread_item = make_proc_test_item( itype_id( "bread" ),
                                 proc::schema_id( "sandwich" ),
                                 "sandwich:bread" );
    const auto proc_bread = proc::normalize_part_fact( *proc_bread_item, { .ix = 20 } );
    CHECK( std::ranges::find( proc_bread.tag, "dish" ) != proc_bread.tag.end() );
    CHECK( std::ranges::find( proc_bread.tag, "bread" ) == proc_bread.tag.end() );
    CHECK_FALSE( proc_bread.proc.empty() );

    const auto proc_carrot_item = make_proc_test_item( itype_id( "carrot" ), proc::schema_id( "stew" ),
                                  "stew:carrot" );
    const auto proc_carrot = proc::normalize_part_fact( *proc_carrot_item, { .ix = 21 } );
    CHECK( std::ranges::find( proc_carrot.tag, "dish" ) != proc_carrot.tag.end() );
    CHECK( std::ranges::find( proc_carrot.tag, "veg" ) == proc_carrot.tag.end() );
    CHECK_FALSE( proc_carrot.proc.empty() );
}
