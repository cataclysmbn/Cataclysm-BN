#include "catch/catch.hpp"

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
