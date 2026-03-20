#include "catch/catch.hpp"

#include <ranges>

#include "item.h"
#include "itype.h"
#include "proc_fact.h"

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
